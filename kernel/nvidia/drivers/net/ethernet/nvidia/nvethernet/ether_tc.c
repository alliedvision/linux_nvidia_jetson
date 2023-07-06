/*
 * Copyright (c) 2021-2023, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ether_linux.h"

#if (KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE)
int ether_tc_setup_taprio(struct ether_priv_data *pdata,
			  struct tc_taprio_qopt_offload *qopt)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	unsigned int fpe_required = OSI_DISABLE;
	struct osi_ioctl fpe_ioctl_data = {};
	struct osi_ioctl est_ioctl_data = {};
	unsigned long cycle_time = 0x0U;
	/* Hardcode width base on current HW config, input parameter validation
	 * will be done by OSI code any way
	 */
	/* total GCL enatry will have 32 valid bits */
	unsigned int wid_val = OSI_MAX_32BITS;
	unsigned int gates = 0x0U;
	/* time is 24 bit long */
	unsigned int wid = 24U;
	struct timespec64 time;
	unsigned long ctr;
	int i, ret = 0;

	if (qopt == NULL) {
		netdev_err(pdata->ndev, "invalid input argument\n");
		return -EINVAL;
	}

	if ((osi_core->hw_feature != OSI_NULL) &&
	    (pdata->hw_feat.est_sel == OSI_DISABLE)) {
		netdev_err(pdata->ndev, "EST not supported in HW\n");
		ret = -EOPNOTSUPP;
		goto done;
	}

	if (qopt->num_entries >= OSI_GCL_SIZE_256) {
		netdev_err(pdata->ndev, "invalid number of GCL entries\n");
		ret = -ERANGE;
		goto done;
	}

	if (!qopt->base_time) {
		netdev_err(pdata->ndev, "invalid base time\n");
		ret = -ERANGE;
		goto done;
	}

	if (!qopt->cycle_time) {
		netdev_err(pdata->ndev, "invalid Cycle time\n");
		ret = -ERANGE;
		goto done;
	}

	memset(&est_ioctl_data.est, 0x0, sizeof(struct osi_est_config));
	memset(&est_ioctl_data.fpe, 0x0, sizeof(struct osi_fpe_config));

	/* This code is to disable TSN, User space is asking to disable
	 */
	if (!qopt->enable) {
		goto disable;
	}

	est_ioctl_data.est.llr = qopt->num_entries;
	est_ioctl_data.est.en_dis = qopt->enable;

	for (i = 0U; i < est_ioctl_data.est.llr; i++) {
		cycle_time = qopt->entries[i].interval;
		gates = qopt->entries[i].gate_mask;

		switch (qopt->entries[i].command) {
		case TC_TAPRIO_CMD_SET_GATES:
			if (fpe_required == OSI_ENABLE) {
				netdev_err(pdata->ndev,
					   "with set-and-hold/release, only set command is not expected\n");
				ret = -EINVAL;
				goto done;
			}
			break;
		case TC_TAPRIO_CMD_SET_AND_HOLD:
			gates |= OSI_BIT(0);
			fpe_required = OSI_ENABLE;
			break;
		case TC_TAPRIO_CMD_SET_AND_RELEASE:
			gates &= ~OSI_BIT(0);
			fpe_required = OSI_ENABLE;
			break;
		default:
			netdev_err(pdata->ndev, "invalid command\n");
			ret = -EOPNOTSUPP;
			goto done;
		}

		est_ioctl_data.est.gcl[i] = cycle_time | (gates << wid);
		if (est_ioctl_data.est.gcl[i] > wid_val) {
			netdev_err(pdata->ndev, "invalid GCL creation\n");
			ret = -EINVAL;
			goto done;
		}
	}

	/* Adjust for real system time TODO better to have
	 * some offset to avoid BTRE
	 */
	time = ktime_to_timespec64(qopt->base_time);
	est_ioctl_data.est.btr[0] = (unsigned int)time.tv_nsec;
	est_ioctl_data.est.btr[1] = (unsigned int)time.tv_sec;
	est_ioctl_data.est.btr_offset[0] = 0;
	est_ioctl_data.est.btr_offset[1] = 0;

	ctr = qopt->cycle_time;
	est_ioctl_data.est.ctr[0] = do_div(ctr, NSEC_PER_SEC);
	est_ioctl_data.est.ctr[1] = (unsigned int)ctr;

	if ((!pdata->hw_feat.fpe_sel) && (fpe_required == OSI_ENABLE)) {
		netdev_err(pdata->ndev, "FPE not supported in HW\n");
		ret = -EOPNOTSUPP;
		goto done;
	}

	if (fpe_required == OSI_ENABLE) {
		fpe_ioctl_data.fpe.rq = osi_core->residual_queue;
		fpe_ioctl_data.fpe.tx_queue_preemption_enable = 0x1;
		fpe_ioctl_data.cmd = OSI_CMD_CONFIG_FPE;
		ret = osi_handle_ioctl(osi_core, &fpe_ioctl_data);
		if (ret < 0) {
			netdev_err(pdata->ndev,
				   "failed to enable Frame Preemption\n");
			goto done;
		} else {
			netdev_info(pdata->ndev, "configured FPE\n");
		}
	}

	est_ioctl_data.cmd = OSI_CMD_CONFIG_EST;
	ret = osi_handle_ioctl(osi_core, &est_ioctl_data);
	if (ret < 0) {
		netdev_err(pdata->ndev, "failed to configure EST\n");
		goto disable;
	}

	netdev_info(pdata->ndev, "configured EST\n");
	return 0;

disable:
	est_ioctl_data.est.en_dis = false;
	est_ioctl_data.cmd = OSI_CMD_CONFIG_EST;
	ret = osi_handle_ioctl(osi_core, &est_ioctl_data);
	if ((ret >= 0) && (fpe_required == OSI_ENABLE)) {
		fpe_ioctl_data.fpe.tx_queue_preemption_enable = 0x0;
		fpe_ioctl_data.cmd = OSI_CMD_CONFIG_FPE;
		ret = osi_handle_ioctl(osi_core, &fpe_ioctl_data);
	}

done:
	return ret;
}

int ether_tc_setup_cbs(struct ether_priv_data *pdata,
		       struct tc_cbs_qopt_offload *qopt)
{
	struct osi_core_priv_data *osi_core = pdata->osi_core;
	struct phy_device *phydev = pdata->phydev;
	struct osi_ioctl ioctl_data = {};
	int queue = qopt->queue;
	unsigned int multiplier, speed_div;
	unsigned long  value;
	int speed;

	/* Queue 0 is not AVB capable */
	if (queue <= 0) {
		netdev_err(pdata->ndev, "%s() invalid queue\n", __func__);
		return -EINVAL;
	}

	if (phydev != NULL) {
		speed = phydev->speed;
	} else {
		speed = pdata->speed;
	}

	switch (speed) {
	case OSI_SPEED_10000:
		multiplier = MULTIPLIER_32;
		speed_div = OSI_SPEED_10000 * ETH_1K;
		break;
	case OSI_SPEED_5000:
		multiplier = MULTIPLIER_32;
		speed_div = OSI_SPEED_5000 * ETH_1K;
		break;
	case OSI_SPEED_2500:
		multiplier = MULTIPLIER_8;
		speed_div = OSI_SPEED_2500 * ETH_1K;
		break;
	case OSI_SPEED_1000:
		multiplier = MULTIPLIER_8;
		speed_div = OSI_SPEED_1000 * ETH_1K;
		break;
	case OSI_SPEED_100:
		multiplier = MULTIPLIER_4;
		speed_div = OSI_SPEED_100 * ETH_1K;
		break;
	default:
		netdev_err(pdata->ndev, "invalid speed\n");
		return -EINVAL;
	}

	ioctl_data.avb.qindex = (unsigned int)queue;
	ioctl_data.avb.tcindex = (unsigned int)queue;

	if (qopt->enable) {
		ioctl_data.avb.algo = OSI_MTL_TXQ_AVALG_CBS;
		ioctl_data.avb.oper_mode = OSI_MTL_QUEUE_AVB;
		ioctl_data.avb.credit_control = OSI_ENABLE;
	} else {
	/* For EQOS harware library code use internally SP(0) and
	   For MGBE harware library code use internally ETS(2) if
	   algo != CBS. */
		ioctl_data.avb.algo = OSI_MTL_TXQ_AVALG_SP;
		ioctl_data.avb.oper_mode = OSI_MTL_QUEUE_ENABLE;
		ioctl_data.avb.credit_control = OSI_DISABLE;
	}

	/* Final adjustments for HW */
	value = div_s64(qopt->idleslope * 1024ll * multiplier, speed_div);
	ioctl_data.avb.idle_slope = (unsigned long)value;

	value = div_s64(-qopt->sendslope * 1024ll * multiplier, speed_div);
	ioctl_data.avb.send_slope = (unsigned long)value;

	value = qopt->hicredit * 1024ll * 8;
	ioctl_data.avb.hi_credit = (unsigned long)value;

	value = qopt->locredit * 1024ll * 8;
	ioctl_data.avb.low_credit = (unsigned long)value;

	ioctl_data.cmd = OSI_CMD_SET_AVB;

	return osi_handle_ioctl(osi_core, &ioctl_data);
}
#endif
