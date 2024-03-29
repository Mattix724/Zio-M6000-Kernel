/* ehci-msm.c - HSUSB Host Controller Driver Implementation
 *
 * Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
 *
 * Partly derived from ehci-fsl.c and ehci-hcd.c
 * Copyright (c) 2000-2004 by David Brownell
 * Copyright (c) 2005 MontaVista Software
 *
 * All source code in this file is licensed under the following license except
 * where indicated.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can find it at http://www.fsf.org
 */

#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/clk.h>
#include <linux/spinlock.h>

#include <mach/board.h>
#include <mach/rpc_hsusb.h>
#include <mach/msm_hsusb.h>
#include <mach/msm_hsusb_hw.h>
#include <mach/msm_otg.h>
#include <mach/clk.h>
#include <linux/wakelock.h>
#include <linux/pm_qos_params.h>

#include <mach/msm72k_otg.h>

#define MSM_USB_BASE (hcd->regs)

struct msmusb_hcd {
	struct ehci_hcd ehci;
	struct clk *clk;
	struct clk *pclk;
	unsigned in_lpm;
	struct work_struct lpm_exit_work;
	spinlock_t lock;
	struct wake_lock wlock;
	unsigned int clk_enabled;
	struct msm_usb_host_platform_data *pdata;
	unsigned running;
	struct otg_transceiver *xceiv;
	struct work_struct otg_work;
	unsigned flags;
	struct msm_otg_ops otg_ops;
};

static inline struct msmusb_hcd *hcd_to_mhcd(struct usb_hcd *hcd)
{
	return (struct msmusb_hcd *) (hcd->hcd_priv);
}

static inline struct usb_hcd *mhcd_to_hcd(struct msmusb_hcd *mhcd)
{
	return container_of((void *) mhcd, struct usb_hcd, hcd_priv);
}

static void msm_xusb_pm_qos_update(struct msmusb_hcd *mhcd, int vote)
{
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);

	if (PHY_TYPE(mhcd->pdata->phy_info) == USB_PHY_SERIAL_PMIC)
		goto vote_for_axi;

	if (!depends_on_axi_freq(mhcd->xceiv))
		return;

vote_for_axi:
	if (vote) {
		pm_qos_update_requirement(PM_QOS_SYSTEM_BUS_FREQ,
				(char *)hcd->self.bus_name,
				 MSM_AXI_MAX_FREQ);
	} else {
		pm_qos_update_requirement(PM_QOS_SYSTEM_BUS_FREQ,
				(char *) hcd->self.bus_name,
				PM_QOS_DEFAULT_VALUE);
	}
}

static void msm_xusb_enable_clks(struct msmusb_hcd *mhcd)
{
	struct msm_usb_host_platform_data *pdata = mhcd->pdata;

	if (mhcd->clk_enabled)
		return;

	switch (PHY_TYPE(pdata->phy_info)) {
	case USB_PHY_INTEGRATED:
		/* OTG driver takes care of clock management */
		break;
	case USB_PHY_SERIAL_PMIC:
		clk_enable(mhcd->clk);
		clk_enable(mhcd->pclk);
		break;
	default:
		pr_err("%s: undefined phy type ( %X ) \n", __func__,
						pdata->phy_info);
		return;
	}
	mhcd->clk_enabled = 1;
}

static void msm_xusb_disable_clks(struct msmusb_hcd *mhcd)
{
	struct msm_usb_host_platform_data *pdata = mhcd->pdata;

	if (!mhcd->clk_enabled)
		return;

	switch (PHY_TYPE(pdata->phy_info)) {
	case USB_PHY_INTEGRATED:
		/* OTG driver takes care of clock management */
		break;
	case USB_PHY_SERIAL_PMIC:
		clk_disable(mhcd->clk);
		clk_disable(mhcd->pclk);
		break;
	default:
		pr_err("%s: undefined phy type ( %X ) \n", __func__,
						pdata->phy_info);
		return;
	}
	mhcd->clk_enabled = 0;

}

static int usb_wakeup_phy(struct usb_hcd *hcd)
{
	struct msmusb_hcd *mhcd = hcd_to_mhcd(hcd);
	struct msm_usb_host_platform_data *pdata = mhcd->pdata;
	int ret = -ENODEV;

	switch (PHY_TYPE(pdata->phy_info)) {
	case USB_PHY_INTEGRATED:
		break;
	case USB_PHY_SERIAL_PMIC:
		ret = msm_fsusb_resume_phy();
		break;
	default:
		pr_err("%s: undefined phy type ( %X ) \n", __func__,
						pdata->phy_info);
	}

	return ret;
}

static int usb_suspend_phy(struct usb_hcd *hcd)
{
	int ret;
	struct msmusb_hcd *mhcd = hcd_to_mhcd(hcd);
	struct msm_usb_host_platform_data *pdata = mhcd->pdata;

	switch (PHY_TYPE(pdata->phy_info)) {
	case USB_PHY_INTEGRATED:
		break;
	case USB_PHY_SERIAL_PMIC:
		ret = msm_fsusb_set_remote_wakeup();
		ret = msm_fsusb_suspend_phy();
		break;
	default:
		pr_err("%s: undefined phy type ( %X ) \n", __func__,
						pdata->phy_info);
		ret = -ENODEV;
		break;
	}

	return ret;
}

static int usb_lpm_enter(struct usb_hcd *hcd)
{
	struct device *dev = container_of((void *)hcd, struct device,
							platform_data);
	struct msmusb_hcd *mhcd = hcd_to_mhcd(hcd);

	disable_irq(hcd->irq);
	if (mhcd->in_lpm) {
		pr_info("%s: already in lpm. nothing to do\n", __func__);
		enable_irq(hcd->irq);
		return 0;
	}

	if (HC_IS_RUNNING(hcd->state)) {
		pr_info("%s: can't enter into lpm. controller is runnning\n",
			__func__);
		enable_irq(hcd->irq);
		return -1;
	}

	pr_info("%s: lpm enter procedure started\n", __func__);

	mhcd->in_lpm = 1;

	if (usb_suspend_phy(hcd)) {
		mhcd->in_lpm = 0;
		enable_irq(hcd->irq);
		pr_info("phy suspend failed\n");
		pr_info("%s: lpm enter procedure end\n", __func__);
		return -1;
	}

	msm_xusb_disable_clks(mhcd);

	if (mhcd->xceiv && mhcd->xceiv->set_suspend)
		mhcd->xceiv->set_suspend(mhcd->xceiv, 1);

	if (device_may_wakeup(dev))
		enable_irq_wake(hcd->irq);
	enable_irq(hcd->irq);
	pr_info("%s: lpm enter procedure end\n", __func__);
	return 0;
}

void usb_lpm_exit_w(struct work_struct *work)
{
	struct msmusb_hcd *mhcd = container_of((void *) work,
			struct msmusb_hcd, lpm_exit_work);

	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);

	struct device *dev = container_of((void *)hcd, struct device,
							platform_data);
	msm_xusb_enable_clks(mhcd);


	if (usb_wakeup_phy(hcd)) {
		pr_err("fatal error: cannot bring phy out of lpm\n");
		return;
	}

	/* If resume signalling finishes before lpm exit, PCD is not set in
	 * USBSTS register. Drive resume signal to the downstream device now
	 * so that EHCI can process the upcoming port change interrupt.*/

	writel(readl(USB_PORTSC) | PORTSC_FPR, USB_PORTSC);

	if (mhcd->xceiv && mhcd->xceiv->set_suspend)
		mhcd->xceiv->set_suspend(mhcd->xceiv, 0);

	if (device_may_wakeup(dev))
		disable_irq_wake(hcd->irq);
	enable_irq(hcd->irq);
}

static void usb_lpm_exit(struct usb_hcd *hcd)
{
	unsigned long flags;
	struct msmusb_hcd *mhcd = hcd_to_mhcd(hcd);

	spin_lock_irqsave(&mhcd->lock, flags);
	if (!mhcd->in_lpm) {
		spin_unlock_irqrestore(&mhcd->lock, flags);
		return;
	}
	mhcd->in_lpm = 0;
	disable_irq_nosync(hcd->irq);
	schedule_work(&mhcd->lpm_exit_work);
	spin_unlock_irqrestore(&mhcd->lock, flags);
}

#ifdef CONFIG_PM

static int ehci_msm_bus_suspend(struct usb_hcd *hcd)
{
	int ret;
	struct msmusb_hcd *mhcd = hcd_to_mhcd(hcd);

	ret = ehci_bus_suspend(hcd);
	if (ret) {
		pr_err("ehci_bus suspend faield\n");
		return ret;
	}
	if (PHY_TYPE(mhcd->pdata->phy_info) == USB_PHY_INTEGRATED)
		ret = otg_set_suspend(mhcd->xceiv, 1);
	else
		ret = usb_lpm_enter(hcd);

	wake_unlock(&mhcd->wlock);
	return ret;
}

static int ehci_msm_bus_resume(struct usb_hcd *hcd)
{
	struct msmusb_hcd *mhcd = hcd_to_mhcd(hcd);

	wake_lock(&mhcd->wlock);

	if (PHY_TYPE(mhcd->pdata->phy_info) == USB_PHY_INTEGRATED) {
		otg_set_suspend(mhcd->xceiv, 0);
	} else { /* PMIC serial phy */
		usb_lpm_exit(hcd);
		if (cancel_work_sync(&(mhcd->lpm_exit_work)))
			usb_lpm_exit_w(&mhcd->lpm_exit_work);
	}

	return ehci_bus_resume(hcd);

}

#else

#define ehci_msm_bus_suspend NULL
#define ehci_msm_bus_resume NULL

#endif	/* CONFIG_PM */

static int ehci_msm_reset(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci = hcd_to_ehci(hcd);
	int retval;

	ehci->caps = USB_CAPLENGTH;
	ehci->regs = USB_CAPLENGTH +
		HC_LENGTH(ehci_readl(ehci, &ehci->caps->hc_capbase));

	/* cache the data to minimize the chip reads*/
	ehci->hcs_params = ehci_readl(ehci, &ehci->caps->hcs_params);

	retval = ehci_init(hcd);
	if (retval)
		return retval;

	hcd->has_tt = 1;
	ehci->sbrn = HCD_USB2;

	retval = ehci_reset(ehci);

	/* SW workaround for USB stability issues*/
	writel(0x0, USB_AHB_MODE);
	writel(0x0, USB_AHB_BURST);

	return retval;
}

#define PTS_VAL(x) (PHY_TYPE(x) == USB_PHY_SERIAL_PMIC) ? PORTSC_PTS_SERIAL : \
							PORTSC_PTS_ULPI

static int ehci_msm_run(struct usb_hcd *hcd)
{
	struct ehci_hcd *ehci  = hcd_to_ehci(hcd);
	struct msmusb_hcd *mhcd = hcd_to_mhcd(hcd);
	int             retval = 0;
	int     	port   = HCS_N_PORTS(ehci->hcs_params);
	u32 __iomem     *reg_ptr;
	u32             hcc_params;
	struct msm_usb_host_platform_data *pdata = mhcd->pdata;

	hcd->uses_new_polling = 1;
	hcd->poll_rh = 0;

	/* set hostmode */
	reg_ptr = (u32 __iomem *)(((u8 __iomem *)ehci->regs) + USBMODE);
	ehci_writel(ehci, (USBMODE_VBUS | USBMODE_SDIS), reg_ptr);

	/* port configuration - phy, port speed, port power, port enable */
	while (port--)
		ehci_writel(ehci, (PTS_VAL(pdata->phy_info) | PORT_POWER |
				PORT_PE), &ehci->regs->port_status[port]);

	ehci_writel(ehci, ehci->periodic_dma, &ehci->regs->frame_list);
	ehci_writel(ehci, (u32)ehci->async->qh_dma, &ehci->regs->async_next);

	hcc_params = ehci_readl(ehci, &ehci->caps->hcc_params);
	if (HCC_64BIT_ADDR(hcc_params))
		ehci_writel(ehci, 0, &ehci->regs->segment);

	ehci->command &= ~(CMD_LRESET|CMD_IAAD|CMD_PSE|CMD_ASE|CMD_RESET);
	ehci->command |= CMD_RUN;
	ehci_writel(ehci, ehci->command, &ehci->regs->command);
	ehci_readl(ehci, &ehci->regs->command); /* unblock posted writes */

	hcd->state = HC_STATE_RUNNING;

	/*Enable appropriate Interrupts*/
	ehci_writel(ehci, INTR_MASK, &ehci->regs->intr_enable);

	return retval;
}

static struct hc_driver msm_hc_driver = {
	.description		= hcd_name,
	.product_desc 		= "Qualcomm On-Chip EHCI Host Controller",
	.hcd_priv_size 		= sizeof(struct msmusb_hcd),

	/*
	 * generic hardware linkage
	 */
	.irq 			= ehci_irq,
	.flags 			= HCD_USB2,

	.reset 			= ehci_msm_reset,
	.start 			= ehci_msm_run,

	.stop			= ehci_stop,
	.shutdown		= ehci_shutdown,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue		= ehci_urb_enqueue,
	.urb_dequeue		= ehci_urb_dequeue,
	.endpoint_disable	= ehci_endpoint_disable,

	/*
	 * scheduling support
	 */
	.get_frame_number	= ehci_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data	= ehci_hub_status_data,
	.hub_control		= ehci_hub_control,
	.bus_suspend		= ehci_msm_bus_suspend,
	.bus_resume		= ehci_msm_bus_resume,
	.relinquish_port	= ehci_relinquish_port,

	.clear_tt_buffer_complete = ehci_clear_tt_buffer_complete,
};

static void msm_hsusb_request_host(void *handle, int request)
{
	struct msmusb_hcd *mhcd = handle;
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);
	struct msm_usb_host_platform_data *pdata = mhcd->pdata;
	struct msm_otg *otg = container_of(mhcd->xceiv, struct msm_otg, otg);

	switch (request) {
	case REQUEST_RESUME:
		usb_hcd_resume_root_hub(hcd);
		break;
	case REQUEST_START:
		if (mhcd->running)
			break;
		wake_lock(&mhcd->wlock);
		msm_xusb_pm_qos_update(mhcd, 1);
		msm_xusb_enable_clks(mhcd);
		if (PHY_TYPE(pdata->phy_info) == USB_PHY_INTEGRATED)
			if (otg->set_clk)
				otg->set_clk(mhcd->xceiv, 1);
		if (pdata->vbus_power)
			pdata->vbus_power(pdata->phy_info, 1);
		if (pdata->config_gpio)
			pdata->config_gpio(1);
		usb_add_hcd(hcd, hcd->irq, IRQF_SHARED);
		mhcd->running = 1;
		if (PHY_TYPE(pdata->phy_info) == USB_PHY_INTEGRATED)
			if (otg->set_clk)
				otg->set_clk(mhcd->xceiv, 0);
		break;
	case REQUEST_STOP:
		if (!mhcd->running)
			break;
		mhcd->running = 0;
		/* come out of lpm before deregistration */
		if (PHY_TYPE(pdata->phy_info) == USB_PHY_SERIAL_PMIC) {
			usb_lpm_exit(hcd);
			if (cancel_work_sync(&(mhcd->lpm_exit_work)))
				usb_lpm_exit_w(&mhcd->lpm_exit_work);
		}
		usb_remove_hcd(hcd);
		if (pdata->config_gpio)
			pdata->config_gpio(0);
		if (pdata->vbus_power)
			pdata->vbus_power(pdata->phy_info, 0);
		msm_xusb_disable_clks(mhcd);
		wake_lock_timeout(&mhcd->wlock, HZ/2);
		msm_xusb_pm_qos_update(mhcd, 0);
		if (PHY_TYPE(pdata->phy_info) == USB_PHY_INTEGRATED) {
			otg->reset(mhcd->xceiv); //Robert, 20100920, KB62_CR673 : Sync to BSP6040
			otg_set_suspend(mhcd->xceiv, 1);
		}
		break;
	}
}

static void msm_hsusb_otg_work(struct work_struct *work)
{
	struct msmusb_hcd *mhcd;

	mhcd = container_of(work, struct msmusb_hcd, otg_work);
	msm_hsusb_request_host((void *)mhcd, mhcd->flags);
}
static void msm_hsusb_start_host(struct usb_bus *bus, int start)
{
	struct usb_hcd *hcd = bus_to_hcd(bus);
	struct msmusb_hcd *mhcd = hcd_to_mhcd(hcd);

	mhcd->flags = start ? REQUEST_START : REQUEST_STOP;
	schedule_work(&mhcd->otg_work);
}

static int msm_xusb_init_phy(struct msmusb_hcd *mhcd)
{
	int ret = -ENODEV;
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);
	struct msm_usb_host_platform_data *pdata = mhcd->pdata;

	switch (PHY_TYPE(pdata->phy_info)) {
	case USB_PHY_INTEGRATED:
		ret = 0;
	case USB_PHY_SERIAL_PMIC:
		msm_xusb_enable_clks(mhcd);
		writel(0, USB_USBINTR);
		ret = msm_fsusb_rpc_init(&mhcd->otg_ops);
		if (!ret)
			msm_fsusb_init_phy();
		msm_xusb_disable_clks(mhcd);
		break;
	default:
		pr_err("%s: undefined phy type ( %X ) \n", __func__,
						pdata->phy_info);
	}

	return ret;
}

static int msm_xusb_rpc_close(struct msmusb_hcd *mhcd)
{
	int retval = -ENODEV;
	struct msm_usb_host_platform_data *pdata = mhcd->pdata;

	switch (PHY_TYPE(pdata->phy_info)) {
	case USB_PHY_INTEGRATED:
		if (!mhcd->xceiv)
			retval = msm_hsusb_rpc_close();
		break;
	case USB_PHY_SERIAL_PMIC:
		retval = msm_fsusb_reset_phy();
		msm_fsusb_rpc_deinit();
		break;
	default:
		pr_err("%s: undefined phy type ( %X ) \n", __func__,
						pdata->phy_info);
	}
	return retval;
}

static int msm_xusb_init_host(struct msmusb_hcd *mhcd)
{
	int ret = 0;
	struct msm_otg *otg;
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);
	struct msm_usb_host_platform_data *pdata = mhcd->pdata;
	struct device *dev = container_of((void *)hcd, struct device,
							platform_data);	

	switch (PHY_TYPE(pdata->phy_info)) {
	case USB_PHY_INTEGRATED:
		msm_hsusb_rpc_connect();

		if (pdata->vbus_init)
			pdata->vbus_init(1);

		/* VBUS might be present. Turn off vbus */
		if (pdata->vbus_power)
			pdata->vbus_power(pdata->phy_info, 0);

		INIT_WORK(&mhcd->otg_work, msm_hsusb_otg_work);
		mhcd->xceiv = otg_get_transceiver();
		if (!mhcd->xceiv)
			return -ENODEV;
		otg = container_of(mhcd->xceiv, struct msm_otg, otg);
		hcd->regs = otg->regs;
		otg->start_host = msm_hsusb_start_host;

		ret = otg_set_host(mhcd->xceiv, &hcd->self);
		break;
	case USB_PHY_SERIAL_PMIC:
		hcd->regs = ioremap(hcd->rsrc_start, hcd->rsrc_len);

		if (!hcd->regs)
			return -EFAULT;
		/* get usb clocks */
		mhcd->clk = clk_get(dev, "usb_hs2_clk");
		if (IS_ERR(mhcd->clk)) {
			iounmap(hcd->regs);
			return PTR_ERR(mhcd->clk);
		}

		mhcd->pclk = clk_get(dev, "usb_hs2_pclk");
		if (IS_ERR(mhcd->pclk)) {
			iounmap(hcd->regs);
			clk_put(mhcd->clk);
			return PTR_ERR(mhcd->pclk);
		}
		mhcd->otg_ops.request = msm_hsusb_request_host;
		mhcd->otg_ops.handle = (void *) mhcd;
		ret = msm_xusb_init_phy(mhcd);
		if (ret < 0) {
			iounmap(hcd->regs);
			clk_put(mhcd->clk);
			clk_put(mhcd->pclk);
		}
		break;
	default:
		pr_err("phy type is bad\n");
	}
	return ret;
}

static int __init ehci_msm_probe(struct platform_device *pdev)
{
	struct usb_hcd *hcd;
	struct resource *res;
	struct msm_usb_host_platform_data *pdata;
	int retval;
	struct msmusb_hcd *mhcd;

	hcd = usb_create_hcd(&msm_hc_driver, &pdev->dev, dev_name(&pdev->dev));
	if (!hcd)
		return  -ENOMEM;

	hcd->irq = platform_get_irq(pdev, 0);
	if (hcd->irq < 0) {
		usb_put_hcd(hcd);
		return hcd->irq;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		usb_put_hcd(hcd);
		return -ENODEV;
	}

	hcd->rsrc_start = res->start;
	hcd->rsrc_len = resource_size(res);

	mhcd = hcd_to_mhcd(hcd);
	spin_lock_init(&mhcd->lock);
	mhcd->in_lpm = 0;
	mhcd->running = 0;
	device_init_wakeup(&pdev->dev, 1);

	pdata = pdev->dev.platform_data;
	if (PHY_TYPE(pdata->phy_info) == USB_PHY_UNDEFINED) {
		usb_put_hcd(hcd);
		return -ENODEV;
	}
	hcd->power_budget = pdata->power_budget;
	mhcd->pdata = pdata;
	INIT_WORK(&mhcd->lpm_exit_work, usb_lpm_exit_w);

	wake_lock_init(&mhcd->wlock, WAKE_LOCK_SUSPEND, dev_name(&pdev->dev));
	pm_qos_add_requirement(PM_QOS_SYSTEM_BUS_FREQ, (char *)dev_name(&pdev->dev),
					PM_QOS_DEFAULT_VALUE);

	retval = msm_xusb_init_host(mhcd);

	if (retval < 0) {
		usb_put_hcd(hcd);
		wake_lock_destroy(&mhcd->wlock);
		pm_qos_remove_requirement(PM_QOS_SYSTEM_BUS_FREQ,
				(char *) dev_name(&pdev->dev));
	}

	return retval;
}

static void msm_xusb_uninit_host(struct msmusb_hcd *mhcd)
{
	struct usb_hcd *hcd = mhcd_to_hcd(mhcd);
	struct msm_usb_host_platform_data *pdata = mhcd->pdata;

	switch (PHY_TYPE(pdata->phy_info)) {
	case USB_PHY_INTEGRATED:
		if (pdata->vbus_init)
			pdata->vbus_init(0);
		otg_set_host(mhcd->xceiv, NULL);
		otg_put_transceiver(mhcd->xceiv);
		cancel_work_sync(&mhcd->otg_work);
		break;
	case USB_PHY_SERIAL_PMIC:
		iounmap(hcd->regs);
		clk_put(mhcd->clk);
		clk_put(mhcd->pclk);
		msm_fsusb_reset_phy();
		msm_fsusb_rpc_deinit();
		break;
	default:
		pr_err("phy type is bad\n");
	}
}
static int __exit ehci_msm_remove(struct platform_device *pdev)
{
	struct usb_hcd *hcd = platform_get_drvdata(pdev);
	struct msmusb_hcd *mhcd = hcd_to_mhcd(hcd);
	int retval = 0;

	device_init_wakeup(&pdev->dev, 0);

	msm_hsusb_request_host((void *)mhcd, REQUEST_STOP);
	msm_xusb_uninit_host(mhcd);
	usb_put_hcd(hcd);
	retval = msm_xusb_rpc_close(mhcd);

	wake_lock_destroy(&mhcd->wlock);
	pm_qos_remove_requirement(PM_QOS_SYSTEM_BUS_FREQ, (char *) dev_name(&pdev->dev));

	return retval;
}

static struct platform_driver ehci_msm_driver = {
	.probe	= ehci_msm_probe,
	.remove	= __exit_p(ehci_msm_remove),
	.driver	= {.name = "msm_hsusb_host"},
};
