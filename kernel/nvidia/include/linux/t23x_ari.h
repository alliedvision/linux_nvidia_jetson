/*
 * Copyright (c) 2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */
#ifndef T23X_ARI_H
#define T23X_ARI_H

/*  ARI Version numbers */
#define TEGRA_ARI_VERSION_MAJOR  8UL
#define TEGRA_ARI_VERSION_MINOR  1UL


/*
 *  ARI Request IDs
 *
 *   TODO: RENUMBER range before finalization
 *   NOTE: for documentation purposes, only documenting gaps
 *   in ranges, to indicate that we know about the missing ids
 *
 *   Require NO LAB Locks
 *   range from 0 - 31
 */
#define TEGRA_ARI_VERSION                      0UL
#define TEGRA_ARI_ECHO                         1UL
#define TEGRA_ARI_NUM_CORES                    2UL
#define TEGRA_ARI_CSTATE_STAT_QUERY            3UL
/* Undefined                                   4 - 28 */
/* Debug Only ARIs at the end of the NO LAB Lock Range */
#define TEGRA_ARI_CORE_DEBUG_RECOVERY          29UL
#define TEGRA_ARI_DSU_DEBUG_RECOVERY           30UL
#define TEGRA_ARI_CLUSTER_WARM_RESET           31UL

/*
 *   Require CORE LAB Lock -- obtained by MTM from ARI
 *   range from 32 - 63
 */
/* UNDEFINED                                   32 */
/* UNDEFINED                                   33 */
#define TEGRA_ARI_ONLINE_CORE                  34UL
#define TEGRA_ARI_ENTER_CSTATE                 35UL
/* UNDEFINED                                   36 */
#define TEGRA_ARI_TRIGGER_ONLINE_IST           37UL
/*
 *   Require CLUSTER and CORE LAB Lock -- obtained by MTM from ARI
 *   range from 64 - 95
 */
/* UNDEFINED                                   64 */
#define TEGRA_ARI_NVFREQ_REQ                   65UL
#define TEGRA_ARI_NVFREQ_FEEDBACK              66UL
#define TEGRA_ARI_CLUSTER_ATCLKEN              67UL
/*
 *   Require CCPLEX, CLUSTER and CORE LAB Lock -- obtained by MTM from ARI
 *   range from 96 - 127
 */
#define TEGRA_ARI_CCPLEX_CACHE_CONTROL         96UL
#define TEGRA_ARI_CCPLEX_CACHE_CLEAN           97UL
/* UNDEFINED                                   98 */
#define TEGRA_ARI_CCPLEX_LATIC_ON              99UL
#define TEGRA_ARI_UPDATE_CROSSOVER             100UL
/* UNDEFINED                                   101 */
#define TEGRA_ARI_CCPLEX_SHUTDOWN              102UL
/* UNDEFINED                                   103 */
#define TEGRA_ARI_CSTATE_INFO                  104UL
#define TEGRA_ARI_IS_SC7_ALLOWED               105UL
/* UNDEFINED                                   106 */
/* UNDEFINED                                   107 */
#define TEGRA_ARI_SECURITY_CONFIG              108UL
#define TEGRA_ARI_UPDATE_CCPLEX_CARVEOUTS      109UL
#define TEGRA_ARI_DDA_CONTROL                  110UL
#define TEGRA_ARI_PERFMON                      111UL
#define TEGRA_ARI_DEBUG_CONFIG                 112UL
#define TEGRA_ARI_CCPLEX_ERROR_RECOVERY_RESET  114UL

/* Values for ARI CSTATE STAT QUERY  */
#define TEGRA_ARI_STAT_QUERY_SC7_ENTRIES        1UL
#define TEGRA_ARI_STAT_QUERY_CC7_ENTRIES        6UL
#define TEGRA_ARI_STAT_QUERY_C7_ENTRIES         14UL
#define TEGRA_ARI_STAT_QUERY_SC7_ENTRY_TIME_SUM 60UL
#define TEGRA_ARI_STAT_QUERY_CC7_ENTRY_TIME_SUM 61UL
#define TEGRA_ARI_STAT_QUERY_C7_ENTRY_TIME_SUM  64UL
#define TEGRA_ARI_STAT_QUERY_SC7_EXIT_TIME_SUM  70UL
#define TEGRA_ARI_STAT_QUERY_CC7_EXIT_TIME_SUM  71UL
#define TEGRA_ARI_STAT_QUERY_C7_EXIT_TIME_SUM   74UL

/* Values for ARI UPDATE CROSSOVER */
#define TEGRA_ARI_CROSSOVER_C7_LOWER_BOUND  0UL
#define TEGRA_ARI_CROSSOVER_CC7_LOWER_BOUND 1UL

/* Values for ARI UPDATE CCPLEX CARVEOUTS */
#define TEGRA_ARI_UPDATE_CCPLEX_CARVEOUTS_ALL 0UL

#endif /* T23X_ARI_H */
