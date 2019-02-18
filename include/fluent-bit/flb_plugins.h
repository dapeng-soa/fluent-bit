/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Fluent Bit
 *  ==========
 *  Copyright (C) 2015 Treasure Data Inc.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifndef FLB_PLUGINS_H
#define FLB_PLUGINS_H

#include <monkey/mk_core.h>
#include <fluent-bit/flb_input.h>
#include <fluent-bit/flb_output.h>
#include <fluent-bit/flb_filter.h>
#include <fluent-bit/flb_config.h>

extern struct flb_input_plugin in_cpu_plugin;
extern struct flb_input_plugin in_mem_plugin;
extern struct flb_input_plugin in_kmsg_plugin;
extern struct flb_input_plugin in_tail_plugin;
extern struct flb_input_plugin in_proc_plugin;
extern struct flb_input_plugin in_disk_plugin;
extern struct flb_input_plugin in_netif_plugin;
extern struct flb_input_plugin in_dummy_plugin;
extern struct flb_input_plugin in_head_plugin;
extern struct flb_input_plugin in_exec_plugin;
extern struct flb_input_plugin in_health_plugin;
extern struct flb_input_plugin in_serial_plugin;
extern struct flb_input_plugin in_stdin_plugin;
extern struct flb_input_plugin in_tcp_plugin;
extern struct flb_input_plugin in_mqtt_plugin;
extern struct flb_input_plugin in_lib_plugin;
extern struct flb_input_plugin in_forward_plugin;
extern struct flb_input_plugin in_random_plugin;
extern struct flb_input_plugin in_syslog_plugin;

extern struct flb_output_plugin out_counter_plugin;
extern struct flb_output_plugin out_es_plugin;
extern struct flb_output_plugin out_exit_plugin;
extern struct flb_output_plugin out_file_plugin;
extern struct flb_output_plugin out_forward_plugin;
extern struct flb_output_plugin out_http_plugin;
extern struct flb_output_plugin out_influxdb_plugin;
extern struct flb_output_plugin out_kafka_rest_plugin;
extern struct flb_output_plugin out_nats_plugin;
extern struct flb_output_plugin out_null_plugin;
extern struct flb_output_plugin out_plot_plugin;
extern struct flb_output_plugin out_splunk_plugin;
extern struct flb_output_plugin out_stdout_plugin;
extern struct flb_output_plugin out_td_plugin;
extern struct flb_output_plugin out_lib_plugin;
extern struct flb_output_plugin out_flowcounter_plugin;

extern struct flb_filter_plugin filter_grep_plugin;
extern struct flb_filter_plugin filter_stdout_plugin;
extern struct flb_filter_plugin filter_throttle_plugin;
extern struct flb_filter_plugin filter_kubernetes_plugin;
extern struct flb_filter_plugin filter_parser_plugin;
extern struct flb_filter_plugin filter_record_modifier_plugin;


void flb_register_plugins(struct flb_config *config)
{
    struct flb_input_plugin *in;
    struct flb_output_plugin *out;
    struct flb_filter_plugin *filter;

    in = &in_cpu_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_mem_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_kmsg_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_tail_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_proc_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_disk_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_netif_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_dummy_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_head_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_exec_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_health_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_serial_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_stdin_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_tcp_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_mqtt_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_lib_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_forward_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_random_plugin;
    mk_list_add(&in->_head, &config->in_plugins);

    in = &in_syslog_plugin;
    mk_list_add(&in->_head, &config->in_plugins);


    out = &out_counter_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_es_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_exit_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_file_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_forward_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_http_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_influxdb_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_kafka_rest_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_nats_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_null_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_plot_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_splunk_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_stdout_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_td_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_lib_plugin;
    mk_list_add(&out->_head, &config->out_plugins);

    out = &out_flowcounter_plugin;
    mk_list_add(&out->_head, &config->out_plugins);


    filter = &filter_grep_plugin;
    mk_list_add(&filter->_head, &config->filter_plugins);

    filter = &filter_stdout_plugin;
    mk_list_add(&filter->_head, &config->filter_plugins);

    filter = &filter_throttle_plugin;
    mk_list_add(&filter->_head, &config->filter_plugins);

    filter = &filter_kubernetes_plugin;
    mk_list_add(&filter->_head, &config->filter_plugins);

    filter = &filter_parser_plugin;
    mk_list_add(&filter->_head, &config->filter_plugins);

    filter = &filter_record_modifier_plugin;
    mk_list_add(&filter->_head, &config->filter_plugins);


}

#endif
