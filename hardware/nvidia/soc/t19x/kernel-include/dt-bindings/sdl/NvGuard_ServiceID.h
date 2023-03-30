/*
 * Copyright (c) 2019-2020, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGUARD_SERVICEID_H
#define NVGUARD_SERVICEID_H

/* *************** SERVICE ID for CCPLEX*************** */
/*  IDs for SW Error Handler */
#define NVGUARD_SERVICE_CCPLEX_MAX_SWERR 0U
/* IDs for Diagnostics Test Services */
/** DESCRIPTION : test for permanent fault diagnostic of FPS in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_FPS_PF_TEST0 0x0120b000U
/** DESCRIPTION : test for permanent fault diagnostic of FPS in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_FPS_PF_TEST1 0x0120b001U
/** DESCRIPTION : test for permanent fault diagnostic of FPS in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_FPS_PF_TEST2 0x0120b002U
/** DESCRIPTION : test for permanent fault diagnostic of FPS in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_FPS_PF_TEST3 0x0120b003U
/** DESCRIPTION : test for permanent fault diagnostic of FPS in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_FPS_PF_TEST4 0x0120b004U
/** DESCRIPTION : test for permanent fault diagnostic of FPS in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_FPS_PF_TEST5 0x0120b005U
/** DESCRIPTION : test for permanent fault diagnostic of FPS in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_FPS_PF_TEST6 0x0120b006U
/** DESCRIPTION : test for permanent fault diagnostic of FPS in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_FPS_PF_TEST7 0x0120b007U
/** DESCRIPTION : test for GIC HW diagnostic in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_GIC_SDL_TEST0 0x0120b008U
/** DESCRIPTION : test for GIC HW diagnostic in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_GIC_SDL_TEST1 0x0120b009U
/** DESCRIPTION : test for GIC HW diagnostic in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_GIC_SDL_TEST2 0x0120b00aU
/** DESCRIPTION : test for GIC HW diagnostic in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_GIC_SDL_TEST3 0x0120b00bU
/** DESCRIPTION : test for GIC HW diagnostic in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_GIC_SDL_TEST4 0x0120b00cU
/** DESCRIPTION : test for GIC HW diagnostic in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_GIC_SDL_TEST5 0x0120b00dU
/** DESCRIPTION : test for GIC HW diagnostic in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_GIC_SDL_TEST6 0x0120b00eU
/** DESCRIPTION : test for GIC HW diagnostic in CCPLEX */
#define NVGUARD_SERVICE_CCPLEX_DIAG_GIC_SDL_TEST7 0x0120b00fU
#define NVGUARD_SERVICE_CCPLEX_MAX_DIAG 16U
/* IDs for HW Error Handlers */
/** DESCRIPTION :   */
#define NVGUARD_SERVICE_CCPLEX_HSMERR_CCPLEX_UE 0x0100b0fcU
/** DESCRIPTION :   */
#define NVGUARD_SERVICE_CCPLEX_HSMERR_CCPLEX_CE 0x0100b1e3U
#define NVGUARD_SERVICE_CCPLEX_MAX_HWERR 2U
/* *************** END SERVICE ID for CCPLEX*************** */

#endif //NVGUARD_SERVICEID_H
