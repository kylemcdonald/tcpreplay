/* $Id:$ */

/*
 * Copyright (c) 2006-2007 Aaron Turner.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of the copyright owners nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include <stdlib.h>
#include <string.h>

#include "dlt_plugins-int.h"
#include "dlt_utils.h"
#include "raw.h"
#include "tcpedit.h"
#include "common.h"
#include "tcpr.h"

/* FIXME: edit these variables to taste */
static char dlt_name[] = "raw";
static char __attribute__((unused)) dlt_prefix[] = "raw";
static u_int16_t dlt_value = DLT_RAW;

/*
 * DLT_RAW is basically a zero length L2 header for IPv4 & IPv6 packets
 */

/*
 * Function to register ourselves.  This function is always called, regardless
 * of what DLT types are being used, so it shouldn't be allocating extra buffers
 * or anything like that (use the dlt_raw_init() function below for that).
 * Tasks:
 * - Create a new plugin struct
 * - Fill out the provides/requires bit masks.  Note:  Only specify which fields are
 *   actually in the header.
 * - Add the plugin to the context's plugin chain
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_raw_register(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    assert(ctx);

    /* create  a new plugin structure */
    plugin = tcpedit_dlt_newplugin();

    /* set what we provide & require  */
    plugin->provides += PLUGIN_MASK_PROTO;

     /* what is our DLT value? */
    plugin->dlt = dlt_value;

    /* set the prefix name of our plugin.  This is also used as the prefix for our options */
    plugin->name = safe_strdup(dlt_prefix);

    /* 
     * Point to our functions, note, you need a function for EVERY method.  
     * Even if it is only an empty stub returning success.
     */
    plugin->plugin_init = dlt_raw_init;
    plugin->plugin_cleanup = dlt_raw_cleanup;
    plugin->plugin_parse_opts = dlt_raw_parse_opts;
    plugin->plugin_decode = dlt_raw_decode;
    plugin->plugin_encode = dlt_raw_encode;
    plugin->plugin_proto = dlt_raw_proto;
    plugin->plugin_l2addr_type = dlt_raw_l2addr_type;
    plugin->plugin_l2len = dlt_raw_l2len;
    plugin->plugin_get_layer3 = dlt_raw_get_layer3;
    plugin->plugin_merge_layer3 = dlt_raw_merge_layer3;

    /* add it to the available plugin list */
    return tcpedit_dlt_addplugin(ctx, plugin);
}

 
/*
 * Initializer function.  This function is called only once, if and only iif
 * this plugin will be utilized.  Remember, if you need to keep track of any state, 
 * store it in your plugin->config, not a global!
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_raw_init(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    raw_config_t *config;
    assert(ctx);
    
    if ((plugin = tcpedit_dlt_getplugin(ctx, dlt_value)) == NULL) {
        tcpedit_seterr(ctx->tcpedit, "Unable to initalize unregistered plugin %s", dlt_name);
        return TCPEDIT_ERROR;
    }
    
    /* allocate memory for our deocde extra data */
    if (sizeof(raw_extra_t) > 0)
        ctx->decoded_extra = safe_malloc(sizeof(raw_extra_t));

    /* allocate memory for our config data */
    if (sizeof(raw_config_t) > 0)
        plugin->config = safe_malloc(sizeof(raw_config_t));
    
    config = (raw_config_t *)plugin->config;
    
    return TCPEDIT_OK; /* success */
}

/*
 * Since this is used in a library, we should manually clean up after ourselves
 * Unless you allocated some memory in dlt_raw_init(), this is just an stub.
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_raw_cleanup(tcpeditdlt_t *ctx)
{
    tcpeditdlt_plugin_t *plugin;
    assert(ctx);

    if ((plugin = tcpedit_dlt_getplugin(ctx, dlt_value)) == NULL) {
        tcpedit_seterr(ctx->tcpedit, "Unable to cleanup unregistered plugin %s", dlt_name);
        return TCPEDIT_ERROR;
    }

    /* FIXME: make this function do something if necessary */
    if (ctx->decoded_extra != NULL) {
        free(ctx->decoded_extra);
        ctx->decoded_extra = NULL;
    }
        
    if (plugin->config != NULL) {
        free(plugin->config);
        plugin->config = NULL;
    }

    return TCPEDIT_OK; /* success */
}

/*
 * This is where you should define all your AutoGen AutoOpts option parsing.
 * Any user specified option should have it's bit turned on in the 'provides'
 * bit mask.
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_raw_parse_opts(tcpeditdlt_t *ctx)
{
    assert(ctx);

    /* no op */
    return TCPEDIT_OK; /* success */
}

/*
 * Function to decode the layer 2 header in the packet.
 * You need to fill out:
 * - ctx->l2len
 * - ctx->srcaddr
 * - ctx->dstaddr
 * - ctx->proto
 * - ctx->decoded_extra
 * Returns: TCPEDIT_ERROR | TCPEDIT_OK | TCPEDIT_WARN
 */
int 
dlt_raw_decode(tcpeditdlt_t *ctx, const u_char *packet, const int pktlen)
{
    int proto;
    assert(ctx);
    assert(packet);
    assert(pktlen > 0);

    if ((proto = dlt_raw_proto(ctx, packet, pktlen)) == TCPEDIT_ERROR)
        return TCPEDIT_ERROR;
        
    ctx->proto = (u_int16_t)proto;
    ctx->l2len = 0;

    return TCPEDIT_OK; /* success */
}

/*
 * Function to encode the layer 2 header back into the packet.
 * Returns: total packet len or TCPEDIT_ERROR
 */
int 
dlt_raw_encode(tcpeditdlt_t *ctx, u_char **packet_ex, int pktlen, __attribute__((unused))tcpr_dir_t dir)
{
    u_char *packet;
    assert(ctx);
    assert(packet_ex);
    assert(pktlen > 0);
    
    packet = *packet_ex;
    assert(packet);
    
    tcpedit_seterr(ctx->tcpedit, "%s", "DLT_RAW plugin does not support packet encoding");
    return TCPEDIT_ERROR; 
}

/*
 * Function returns the Layer 3 protocol type of the given packet, or TCPEDIT_ERROR on error
 */
int 
dlt_raw_proto(tcpeditdlt_t *ctx, const u_char *packet, const int pktlen)
{
    struct tcpr_ipv4_hdr *iphdr;
    assert(ctx);
    assert(packet);
    assert(pktlen > 0);
    int protocol = 0;

    iphdr = (struct tcpr_ipv4_hdr *)packet;
    if (iphdr->ip_v == 0x04) {
        protocol = ETHERTYPE_IP;
    } else if (iphdr->ip_v == 0x06) {
        protocol = ETHERTYPE_IP6;
    } else {
        tcpedit_seterr(ctx->tcpedit, "%s", "Unsupported DLT_RAW packet: doesn't look like IPv4 or IPv6");
        return TCPEDIT_ERROR;
    }
    
    return protocol; 
}

/*
 * Function returns a pointer to the layer 3 protocol header or NULL on error
 */
u_char *
dlt_raw_get_layer3(tcpeditdlt_t *ctx, u_char *packet, const int pktlen)
{
    assert(ctx);
    assert(packet);
    assert(pktlen);
    
    /* raw has a zero byte header, so this is basically a non-op */

    return packet;
}

/*
 * function merges the packet (containing L2 and old L3) with the l3data buffer
 * containing the new l3 data.  Note, if L2 % 4 == 0, then they're pointing to the
 * same buffer, otherwise there was a memcpy involved on strictly aligned architectures
 * like SPARC
 */
u_char *
dlt_raw_merge_layer3(tcpeditdlt_t *ctx, u_char *packet, const int pktlen, u_char *l3data)
{
    assert(ctx);
    assert(packet);
    assert(l3data);
    assert(pktlen);
    
    /* raw has a zero byte header, so this is basically a non-op */
    
    return packet;
}

/* 
 * return the length of the L2 header of the current packet
 */
int
dlt_raw_l2len(tcpeditdlt_t *ctx, const u_char *packet, const int pktlen)
{
    assert(ctx);
    assert(packet);
    assert(pktlen);

    return 0;
}


tcpeditdlt_l2addr_type_t 
dlt_raw_l2addr_type(void)
{
    return NONE;
}