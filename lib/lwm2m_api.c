/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v2.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v20.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    David Navarro, Intel Corporation - initial API and implementation
 *    domedambrosio - Please refer to git log
 *    Simon Bernard - Please refer to git log
 *    Toby Jaffey - Please refer to git log
 *    Julien Vermillard - Please refer to git log
 *    Bosch Software Innovations GmbH - Please refer to git log
 *    Christian Renz - Please refer to git log
 *    Scott Bertin, AMETEK, Inc. - Please refer to git log
 *
 *******************************************************************************/

/*
 Copyright (c) 2013, 2014 Intel Corporation

 Redistribution and use in source and binary forms, with or without modification,
 are permitted provided that the following conditions are met:

     * Redistributions of source code must retain the above copyright notice,
       this list of conditions and the following disclaimer.
     * Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
     * Neither the name of Intel Corporation nor the names of its contributors
       may be used to endorse or promote products derived from this software
       without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 THE POSSIBILITY OF SUCH DAMAGE.

 David Navarro <david.navarro@intel.com>

*/

#include "liblwm2m.h"
#include "lwm2m_api.h"
#include "hashmap.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <inttypes.h>
#include <pthread.h>

#include "commandline.h"
#include "connection.h"
#include "hashmap.h"
#include "log.h"

#define MAX_PACKET_SIZE 1024

static lwm2m_context_t *lwm2mH = NULL;
static struct hashmap *connMap;
static struct hashmap *clientByEndpointMap;
static pthread_mutex_t lock;
static int g_quit = 0;
static Callbacks *lwm2m_callbacks = NULL;

typedef struct
{
    uint16_t clientId;
    char *endpoint;
    size_t endpointLen;
} endpoint_t;

static void prv_print_error(uint8_t status)
{
    fprintf(stdout, "Error: ");
    print_status(stdout, status);
    fprintf(stdout, "\r\n");
}

static const char *prv_dump_version(lwm2m_version_t version)
{
    switch (version)
    {
    case VERSION_MISSING:
        return "Missing";
    case VERSION_UNRECOGNIZED:
        return "Unrecognized";
    case VERSION_1_0:
        return "1.0";
    case VERSION_1_1:
        return "1.1";
    default:
        return "";
    }
}

static void prv_dump_binding(lwm2m_binding_t binding)
{
    if (BINDING_UNKNOWN == binding)
    {
        fprintf(stdout, "\tbinding: \"Not specified\"\r\n");
    }
    else
    {
        const struct bindingTable
        {
            lwm2m_binding_t binding;
            const char *text;
        } bindingTable[] =
            {
                {BINDING_U, "UDP"},
                {BINDING_T, "TCP"},
                {BINDING_S, "SMS"},
                {BINDING_N, "Non-IP"},
                {BINDING_Q, "queue mode"},
            };
        size_t i;
        bool oneSeen = false;
        fprintf(stdout, "\tbinding: \"");
        for (i = 0; i < sizeof(bindingTable) / sizeof(bindingTable[0]); i++)
        {
            if ((binding & bindingTable[i].binding) != 0)
            {
                if (oneSeen)
                {
                    fprintf(stdout, ", %s", bindingTable[i].text);
                }
                else
                {
                    fprintf(stdout, "%s", bindingTable[i].text);
                    oneSeen = true;
                }
            }
        }
        fprintf(stdout, "\"\r\n");
    }
}

static void prv_notify_callback(uint16_t clientID,
                                lwm2m_uri_t *uriP,
                                int count,
                                lwm2m_media_type_t format,
                                uint8_t *data,
                                int dataLength,
                                void *userData);

static void prv_result_callback(uint16_t clientID,
                                lwm2m_uri_t *uriP,
                                int status,
                                lwm2m_media_type_t format,
                                uint8_t *data,
                                int dataLength,
                                void *userData);

static void prv_dump_client(lwm2m_client_t *targetP, lwm2m_context_t *ctx, Callbacks *cb)
{
    lwm2m_client_object_t *objectP;

    fprintf(stdout, "Client #%d:\r\n", targetP->internalID);
    fprintf(stdout, "\tname: \"%s\"\r\n", targetP->name);
    fprintf(stdout, "\tversion: \"%s\"\r\n", prv_dump_version(targetP->version));
    prv_dump_binding(targetP->binding);
    if (targetP->msisdn)
        fprintf(stdout, "\tmsisdn: \"%s\"\r\n", targetP->msisdn);
    if (targetP->altPath)
        fprintf(stdout, "\talternative path: \"%s\"\r\n", targetP->altPath);
    fprintf(stdout, "\tlifetime: %d sec\r\n", targetP->lifetime);
    fprintf(stdout, "\tobjects: ");

    for (objectP = targetP->objectList; objectP != NULL; objectP = objectP->next)
    {
        if (objectP->instanceList == NULL)
        {
            fprintf(stdout, "/%d, ", objectP->id);
        }
        else
        {
            lwm2m_list_t *instanceP;
            for (instanceP = objectP->instanceList; instanceP != NULL; instanceP = instanceP->next)
            {
                lwm2m_uri_t uri;
                memset(&uri, 0xFF, sizeof(lwm2m_uri_t));
                uri.objectId = objectP->id;
                uri.instanceId = instanceP->id;
                fprintf(stdout, "Resource /%d/%d\n", uri.objectId, uri.instanceId);
            }
        }
    }
}

static void prv_output_clients(char *buffer,
                               void *user_data)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)user_data;
    lwm2m_client_t *targetP;

    targetP = lwm2mH->clientListX;

    if (targetP == NULL)
    {
        fprintf(stdout, "No client.\r\n");
        return;
    }

    for (targetP = lwm2mH->clientListX; targetP != NULL; targetP = targetP->next)
    {
        prv_dump_client(targetP, NULL, NULL);
    }
}

static int prv_read_id(char *buffer,
                       uint16_t *idP)
{
    int nb;
    int value;

    nb = sscanf(buffer, "%d", &value);
    if (nb == 1)
    {
        if (value < 0 || value > LWM2M_MAX_ID)
        {
            nb = 0;
        }
        else
        {
            *idP = value;
        }
    }

    return nb;
}

static void prv_printUri(const lwm2m_uri_t *uriP)
{
    fprintf(stdout, "/%d", uriP->objectId);
    if (LWM2M_URI_IS_SET_INSTANCE(uriP))
        fprintf(stdout, "/%d", uriP->instanceId);
    else if (LWM2M_URI_IS_SET_RESOURCE(uriP))
        fprintf(stdout, "/");
    if (LWM2M_URI_IS_SET_RESOURCE(uriP))
        fprintf(stdout, "/%d", uriP->resourceId);
#ifndef LWM2M_VERSION_1_0
    else if (LWM2M_URI_IS_SET_RESOURCE_INSTANCE(uriP))
        fprintf(stdout, "/");
    if (LWM2M_URI_IS_SET_RESOURCE_INSTANCE(uriP))
        fprintf(stdout, "/%d", uriP->resourceInstanceId);
#endif
}

static void prv_result_callback(uint16_t clientID,
                                lwm2m_uri_t *uriP,
                                int status,
                                lwm2m_media_type_t format,
                                uint8_t *data,
                                int dataLength,
                                void *userData)
{
    return;
    fprintf(stdout, "\r\nClient #%d ", clientID);
    prv_printUri(uriP);
    fprintf(stdout, " : ");
    print_status(stdout, status);
    fprintf(stdout, "\r\n");

    output_data(stdout, format, data, dataLength, 1);

    fprintf(stdout, "\r\n> ");
    fflush(stdout);
}

static void prv_notify_callback(uint16_t clientID,
                                lwm2m_uri_t *uriP,
                                int count,
                                lwm2m_media_type_t format,
                                uint8_t *data,
                                int dataLength,
                                void *userData)
{
    lwm2m_client_t *clientP = NULL;
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)userData;
    ZF_LOGD("Notify from client #%d ", clientID);
    // prv_printUri(uriP);
    ZF_LOGD(" number %d\n", count);
    // output_data(stdout, format, data, dataLength, 1);
    ZF_LOGD("\n> ");
    if (lwm2m_callbacks != NULL && lwm2m_callbacks->notifyCallback != NULL)
    {
        clientP = lookup_client(lwm2mH, clientID);
        if (clientP == NULL)
        {
            ZF_LOGD("Client not found in cache #%d ", clientID);
        }
        else
        {
            lwm2m_callbacks->notifyCallback(clientP->name, data, dataLength);
        }
    }
    fflush(stdout);
}

static void prv_read_client(char *buffer,
                            void *user_data)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)user_data;
    uint16_t clientId;
    lwm2m_uri_t uri;
    char *end = NULL;
    int result;

    result = prv_read_id(buffer, &clientId);
    if (result != 1)
        goto syntax_error;

    buffer = get_next_arg(buffer, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    result = lwm2m_stringToUri(buffer, end - buffer, &uri);
    if (result == 0)
        goto syntax_error;

    if (!check_end_of_args(end))
        goto syntax_error;

    result = lwm2m_dm_read(lwm2mH, clientId, &uri, prv_result_callback, NULL);

    if (result == 0)
    {
        fprintf(stdout, "OK");
    }
    else
    {
        prv_print_error(result);
    }
    return;

syntax_error:
    fprintf(stdout, "Syntax error !");
}

static void prv_discover_client(char *buffer,
                                void *user_data)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)user_data;
    uint16_t clientId;
    lwm2m_uri_t uri;
    char *end = NULL;
    int result;

    result = prv_read_id(buffer, &clientId);
    if (result != 1)
        goto syntax_error;

    buffer = get_next_arg(buffer, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    result = lwm2m_stringToUri(buffer, end - buffer, &uri);
    if (result == 0)
        goto syntax_error;

    if (!check_end_of_args(end))
        goto syntax_error;

    result = lwm2m_dm_discover(lwm2mH, clientId, &uri, prv_result_callback, NULL);

    if (result == 0)
    {
        fprintf(stdout, "OK");
    }
    else
    {
        prv_print_error(result);
    }
    return;

syntax_error:
    fprintf(stdout, "Syntax error !");
}

static void prv_do_write_client(char *buffer,
                                void *user_data,
                                bool partialUpdate)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)user_data;
    uint16_t clientId;
    lwm2m_uri_t uri;
    lwm2m_data_t *dataP = NULL;
    int count = 0;
    char *end = NULL;
    int result;

    result = prv_read_id(buffer, &clientId);
    if (result != 1)
        goto syntax_error;

    buffer = get_next_arg(buffer, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    result = lwm2m_stringToUri(buffer, end - buffer, &uri);
    if (result == 0)
        goto syntax_error;

    buffer = get_next_arg(end, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    if (!check_end_of_args(end))
        goto syntax_error;

#ifdef LWM2M_SUPPORT_SENML_JSON
    if (count <= 0)
    {
        count = lwm2m_data_parse(&uri, (uint8_t *)buffer, end - buffer, LWM2M_CONTENT_SENML_JSON, &dataP);
    }
#endif
#ifdef LWM2M_SUPPORT_JSON
    if (count <= 0)
    {
        count = lwm2m_data_parse(&uri, (uint8_t *)buffer, end - buffer, LWM2M_CONTENT_JSON, &dataP);
    }
#endif
    if (count > 0)
    {
        lwm2m_client_t *clientP = NULL;
        // clientP = (lwm2m_client_t *)lwm2m_list_find((lwm2m_list_t *)lwm2mH->clientList, clientId);
        clientP = lookup_client(lwm2mH, clientId);
        if (clientP != NULL)
        {
            lwm2m_media_type_t format = clientP->format;
            uint8_t *serialized;
            int length = lwm2m_data_serialize(&uri,
                                              count,
                                              dataP,
                                              &format,
                                              &serialized);
            if (length > 0)
            {
                result = lwm2m_dm_write(lwm2mH,
                                        clientId,
                                        &uri,
                                        format,
                                        serialized,
                                        length,
                                        partialUpdate,
                                        prv_result_callback,
                                        NULL);
                lwm2m_free(serialized);
            }
            else
            {
                result = COAP_500_INTERNAL_SERVER_ERROR;
            }
        }
        else
        {
            result = COAP_404_NOT_FOUND;
        }
        lwm2m_data_free(count, dataP);
    }
    else if (!partialUpdate)
    {
        result = lwm2m_dm_write(lwm2mH,
                                clientId,
                                &uri,
                                LWM2M_CONTENT_TEXT,
                                (uint8_t *)buffer,
                                end - buffer,
                                partialUpdate,
                                prv_result_callback,
                                NULL);
    }
    else
    {
        goto syntax_error;
    }

    if (result == 0)
    {
        fprintf(stdout, "OK");
    }
    else
    {
        prv_print_error(result);
    }
    return;

syntax_error:
    fprintf(stdout, "Syntax error !");
}

static void prv_write_client(char *buffer,
                             void *user_data)
{
    prv_do_write_client(buffer, user_data, false);
}

static void prv_update_client(char *buffer,
                              void *user_data)
{
    prv_do_write_client(buffer, user_data, true);
}

static void prv_time_client(char *buffer,
                            void *user_data)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)user_data;
    uint16_t clientId;
    lwm2m_uri_t uri;
    char *end = NULL;
    int result;
    lwm2m_attributes_t attr;
    int nb;
    int value;

    result = prv_read_id(buffer, &clientId);
    if (result != 1)
        goto syntax_error;

    buffer = get_next_arg(buffer, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    result = lwm2m_stringToUri(buffer, end - buffer, &uri);
    if (result == 0)
        goto syntax_error;

    memset(&attr, 0, sizeof(lwm2m_attributes_t));
    attr.toSet = LWM2M_ATTR_FLAG_MIN_PERIOD | LWM2M_ATTR_FLAG_MAX_PERIOD;

    buffer = get_next_arg(end, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    nb = sscanf(buffer, "%d", &value);
    if (nb != 1)
        goto syntax_error;
    if (value < 0)
        goto syntax_error;
    attr.minPeriod = value;

    buffer = get_next_arg(end, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    nb = sscanf(buffer, "%d", &value);
    if (nb != 1)
        goto syntax_error;
    if (value < 0)
        goto syntax_error;
    attr.maxPeriod = value;

    if (!check_end_of_args(end))
        goto syntax_error;

    result = lwm2m_dm_write_attributes(lwm2mH, clientId, &uri, &attr, prv_result_callback, NULL);

    if (result == 0)
    {
        fprintf(stdout, "OK");
    }
    else
    {
        prv_print_error(result);
    }
    return;

syntax_error:
    fprintf(stdout, "Syntax error !");
}

static void prv_attr_client(char *buffer,
                            void *user_data)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)user_data;
    uint16_t clientId;
    lwm2m_uri_t uri;
    char *end = NULL;
    int result;
    lwm2m_attributes_t attr;
    int nb;
    float value;

    result = prv_read_id(buffer, &clientId);
    if (result != 1)
        goto syntax_error;

    buffer = get_next_arg(buffer, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    result = lwm2m_stringToUri(buffer, end - buffer, &uri);
    if (result == 0)
        goto syntax_error;

    memset(&attr, 0, sizeof(lwm2m_attributes_t));
    attr.toSet = LWM2M_ATTR_FLAG_LESS_THAN | LWM2M_ATTR_FLAG_GREATER_THAN;

    buffer = get_next_arg(end, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    nb = sscanf(buffer, "%f", &value);
    if (nb != 1)
        goto syntax_error;
    attr.lessThan = value;

    buffer = get_next_arg(end, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    nb = sscanf(buffer, "%f", &value);
    if (nb != 1)
        goto syntax_error;
    attr.greaterThan = value;

    buffer = get_next_arg(end, &end);
    if (buffer[0] != 0)
    {
        nb = sscanf(buffer, "%f", &value);
        if (nb != 1)
            goto syntax_error;
        attr.step = value;

        attr.toSet |= LWM2M_ATTR_FLAG_STEP;
    }

    if (!check_end_of_args(end))
        goto syntax_error;

    result = lwm2m_dm_write_attributes(lwm2mH, clientId, &uri, &attr, prv_result_callback, NULL);

    if (result == 0)
    {
        fprintf(stdout, "OK");
    }
    else
    {
        prv_print_error(result);
    }
    return;

syntax_error:
    fprintf(stdout, "Syntax error !");
}

static void prv_clear_client(char *buffer,
                             void *user_data)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)user_data;
    uint16_t clientId;
    lwm2m_uri_t uri;
    char *end = NULL;
    int result;
    lwm2m_attributes_t attr;

    result = prv_read_id(buffer, &clientId);
    if (result != 1)
        goto syntax_error;

    buffer = get_next_arg(buffer, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    result = lwm2m_stringToUri(buffer, end - buffer, &uri);
    if (result == 0)
        goto syntax_error;

    memset(&attr, 0, sizeof(lwm2m_attributes_t));
    attr.toClear = LWM2M_ATTR_FLAG_LESS_THAN | LWM2M_ATTR_FLAG_GREATER_THAN | LWM2M_ATTR_FLAG_STEP | LWM2M_ATTR_FLAG_MIN_PERIOD | LWM2M_ATTR_FLAG_MAX_PERIOD;

    buffer = get_next_arg(end, &end);
    if (!check_end_of_args(end))
        goto syntax_error;

    result = lwm2m_dm_write_attributes(lwm2mH, clientId, &uri, &attr, prv_result_callback, NULL);

    if (result == 0)
    {
        fprintf(stdout, "OK");
    }
    else
    {
        prv_print_error(result);
    }
    return;

syntax_error:
    fprintf(stdout, "Syntax error !");
}

static void prv_exec_client(char *buffer,
                            void *user_data)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)user_data;
    uint16_t clientId;
    lwm2m_uri_t uri;
    char *end = NULL;
    int result;

    result = prv_read_id(buffer, &clientId);
    if (result != 1)
        goto syntax_error;

    buffer = get_next_arg(buffer, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    result = lwm2m_stringToUri(buffer, end - buffer, &uri);
    if (result == 0)
        goto syntax_error;

    buffer = get_next_arg(end, &end);

    if (buffer[0] == 0)
    {
        result = lwm2m_dm_execute(lwm2mH, clientId, &uri, 0, NULL, 0, prv_result_callback, NULL);
    }
    else
    {
        if (!check_end_of_args(end))
            goto syntax_error;

        result = lwm2m_dm_execute(lwm2mH, clientId, &uri, LWM2M_CONTENT_TEXT, (uint8_t *)buffer, end - buffer, prv_result_callback, NULL);
    }

    if (result == 0)
    {
        fprintf(stdout, "OK");
    }
    else
    {
        prv_print_error(result);
    }
    return;

syntax_error:
    fprintf(stdout, "Syntax error !");
}

static void prv_create_client(char *buffer,
                              void *user_data)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)user_data;
    uint16_t clientId;
    lwm2m_uri_t uri;
    char *end = NULL;
    int result;
    int64_t value;
    lwm2m_data_t *dataP = NULL;
    int size = 0;

    //Get Client ID
    result = prv_read_id(buffer, &clientId);
    if (result != 1)
        goto syntax_error;

    //Get Uri
    buffer = get_next_arg(buffer, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    result = lwm2m_stringToUri(buffer, end - buffer, &uri);
    if (result == 0)
        goto syntax_error;
    if (LWM2M_URI_IS_SET_RESOURCE(&uri))
        goto syntax_error;

    //Get Data to Post
    buffer = get_next_arg(end, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    if (!check_end_of_args(end))
        goto syntax_error;

        // TLV

#ifdef LWM2M_SUPPORT_SENML_JSON
    if (size <= 0)
    {
        size = lwm2m_data_parse(&uri,
                                (uint8_t *)buffer,
                                end - buffer,
                                LWM2M_CONTENT_SENML_JSON,
                                &dataP);
    }
#endif
#ifdef LWM2M_SUPPORT_JSON
    if (size <= 0)
    {
        size = lwm2m_data_parse(&uri,
                                (uint8_t *)buffer,
                                end - buffer,
                                LWM2M_CONTENT_JSON,
                                &dataP);
    }
#endif
    /* Client dependent part   */

    if (size <= 0 && uri.objectId == 31024)
    {
        if (1 != sscanf(buffer, "%" PRId64, &value))
        {
            fprintf(stdout, "Invalid value !");
            return;
        }

        size = 1;
        dataP = lwm2m_data_new(size);
        if (dataP == NULL)
        {
            fprintf(stdout, "Allocation error !");
            return;
        }
        lwm2m_data_encode_int(value, dataP);
        dataP->id = 1;
    }
    /* End Client dependent part*/

    if (LWM2M_URI_IS_SET_INSTANCE(&uri))
    {
        /* URI is only allowed to have the object ID. Wrap the instance in an
         * object instance to get it to the client. */
        int count = size;
        lwm2m_data_t *subDataP = dataP;
        size = 1;
        dataP = lwm2m_data_new(size);
        if (dataP == NULL)
        {
            fprintf(stdout, "Allocation error !");
            lwm2m_data_free(count, subDataP);
            return;
        }
        lwm2m_data_include(subDataP, count, dataP);
        dataP->type = LWM2M_TYPE_OBJECT_INSTANCE;
        dataP->id = uri.instanceId;
        uri.instanceId = LWM2M_MAX_ID;
    }

    //Create
    result = lwm2m_dm_create(lwm2mH, clientId, &uri, size, dataP, prv_result_callback, NULL);
    lwm2m_data_free(size, dataP);

    if (result == 0)
    {
        fprintf(stdout, "OK");
    }
    else
    {
        prv_print_error(result);
    }
    return;

syntax_error:
    fprintf(stdout, "Syntax error !");
}

static void prv_delete_client(char *buffer,
                              void *user_data)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)user_data;
    uint16_t clientId;
    lwm2m_uri_t uri;
    char *end = NULL;
    int result;

    result = prv_read_id(buffer, &clientId);
    if (result != 1)
        goto syntax_error;

    buffer = get_next_arg(buffer, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    result = lwm2m_stringToUri(buffer, end - buffer, &uri);
    if (result == 0)
        goto syntax_error;

    if (!check_end_of_args(end))
        goto syntax_error;

    result = lwm2m_dm_delete(lwm2mH, clientId, &uri, prv_result_callback, NULL);

    if (result == 0)
    {
        fprintf(stdout, "OK");
    }
    else
    {
        prv_print_error(result);
    }
    return;

syntax_error:
    fprintf(stdout, "Syntax error !");
}

static void prv_observe_client(char *buffer,
                               void *user_data)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)user_data;
    uint16_t clientId;
    lwm2m_uri_t uri;
    char *end = NULL;
    int result;

    result = prv_read_id(buffer, &clientId);
    if (result != 1)
        goto syntax_error;

    buffer = get_next_arg(buffer, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    result = lwm2m_stringToUri(buffer, end - buffer, &uri);
    if (result == 0)
        goto syntax_error;

    if (!check_end_of_args(end))
        goto syntax_error;

    result = lwm2m_observe(lwm2mH, clientId, &uri, prv_notify_callback, NULL);

    if (result == 0)
    {
        fprintf(stdout, "OK");
    }
    else
    {
        prv_print_error(result);
    }
    return;

syntax_error:
    fprintf(stdout, "Syntax error !");
}

static void prv_cancel_client(char *buffer,
                              void *user_data)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)user_data;
    uint16_t clientId;
    lwm2m_uri_t uri;
    char *end = NULL;
    int result;

    result = prv_read_id(buffer, &clientId);
    if (result != 1)
        goto syntax_error;

    buffer = get_next_arg(buffer, &end);
    if (buffer[0] == 0)
        goto syntax_error;

    result = lwm2m_stringToUri(buffer, end - buffer, &uri);
    if (result == 0)
        goto syntax_error;

    if (!check_end_of_args(end))
        goto syntax_error;

    result = lwm2m_observe_cancel(lwm2mH, clientId, &uri, prv_result_callback, NULL);

    if (result == 0)
    {
        fprintf(stdout, "OK");
    }
    else
    {
        prv_print_error(result);
    }
    return;

syntax_error:
    fprintf(stdout, "Syntax error !");
}

int registration_callback(lwm2m_context_t *contextP, lwm2m_client_t *targetP, void *userData)
{
    ZF_LOGD("===================== registration_callback ====================== %p %p %p\n",
            contextP, targetP, userData);
    hashmap_set(clientByEndpointMap, &(endpoint_t){
                                         .clientId = targetP->internalID,
                                         .endpoint = targetP->name,
                                         .endpointLen = strlen(targetP->name)});

    return 0;
    // TODO, move to go registration callback
    lwm2m_client_object_t *objectP;
    for (objectP = targetP->objectList; objectP != NULL; objectP = objectP->next)
    {
        if (objectP->instanceList == NULL)
        {
            ZF_LOGD("/%d, ", objectP->id);
        }
        else
        {
            lwm2m_list_t *instanceP;

            for (instanceP = objectP->instanceList; instanceP != NULL; instanceP = instanceP->next)
            {
                if (contextP != NULL)
                {
                    if (objectP->id == 1 && instanceP->id == 0)
                    {
                        continue;
                    }
                    lwm2m_uri_t uri;
                    memset(&uri, 0xFF, sizeof(lwm2m_uri_t));
                    uri.objectId = objectP->id;
                    uri.instanceId = instanceP->id;

                    ZF_LOGD("Send observe to /%d/%d\n", uri.objectId, uri.instanceId);
                    int result = lwm2m_observe(contextP, targetP->internalID, &uri, prv_notify_callback, userData);

                    if (result == 0)
                    {
                        ZF_LOGD("Send observe to /%d/%d - OK\n", uri.objectId, uri.instanceId);
                    }
                    else
                    {
                        ZF_LOGE("Send observe to /%d/%d - ERROR - code: %d\n", objectP->id, instanceP->id, result);
                        prv_print_error(result);
                    }

                    uri.objectId = 3;
                    uri.instanceId = 0;
                    result = lwm2m_dm_read(contextP, targetP->internalID, &uri, prv_result_callback, NULL);
                    if (result == 0)
                    {
                        ZF_LOGD("Read to /%d/%d - OK\n", uri.objectId, uri.instanceId);
                    }
                    else
                    {
                        ZF_LOGE("Read to /%d/%d - ERROR - code: %d\n", objectP->id, instanceP->id, result);
                        prv_print_error(result);
                    }
                }
            }
        }
    }
    return 0;
}

static void prv_monitor_callback(uint16_t clientID,
                                 lwm2m_uri_t *uriP,
                                 int status,
                                 lwm2m_media_type_t format,
                                 uint8_t *data,
                                 int dataLength,
                                 void *userData)
{
    lwm2m_context_t *lwm2mH = (lwm2m_context_t *)userData;
    lwm2m_client_t *targetP;

    switch (status)
    {
    case COAP_201_CREATED:
        fprintf(stdout, "\r\nNew client #%d registered.\r\n", clientID);
        break;

    case COAP_202_DELETED:
        fprintf(stdout, "\r\nClient #%d unregistered.\r\n", clientID);
        break;

    case COAP_204_CHANGED:
        fprintf(stdout, "\r\nClient #%d updated.\r\n", clientID);
        targetP = lookup_client(lwm2mH, clientID);
        prv_dump_client(targetP, NULL, NULL);
        break;

    default:
        fprintf(stdout, "\r\nMonitor callback called with an unknown status: %d.\r\n", status);
        break;
    }
    fflush(stdout);
}

static void prv_quit(char *buffer,
                     void *user_data)
{
    g_quit = 1;
}

void handle_sigint(int signum)
{
    g_quit = 2;
}

void print_usage(void)
{
    fprintf(stderr, "Usage: lwm2mserver [OPTION]\r\n");
    fprintf(stderr, "Launch a LWM2M server on localhost.\r\n\n");
    fprintf(stdout, "Options:\r\n");
    fprintf(stdout, "  -4\t\tUse IPv4 connection. Default: IPv6 connection\r\n");
    fprintf(stdout, "  -l PORT\tSet the local UDP port of the Server. Default: " LWM2M_STANDARD_PORT_STR "\r\n");
    fprintf(stdout, "\r\n");
}

uint64_t conn_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const connection_t *conn = *(connection_t **)item;
    uint64_t rv = hashmap_sip(conn->mapKey, conn->addrLen, seed0, seed1);
    return rv;
}

int conn_compare(const void *a, const void *b, void *udata)
{
    const connection_t *ua = *(connection_t **)a;
    const connection_t *ub = *(connection_t **)b;
    if (ua->addrLen != ub->addrLen)
    {
        return ua->addrLen > ub->addrLen ? -1 : 1;
    }
    return memcmp(ua->mapKey, ub->mapKey, ua->addrLen);
}

uint64_t endpoint_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const endpoint_t *endpoint = (endpoint_t *)item;
    uint64_t rv = hashmap_sip(endpoint->endpoint, endpoint->endpointLen, seed0, seed1);
    return rv;
}

int endpoint_compare(const void *a, const void *b, void *udata)
{
    const endpoint_t *ua = (endpoint_t *)a;
    const endpoint_t *ub = (endpoint_t *)b;
    if (ua->endpointLen != ub->endpointLen)
    {
        return ua->endpointLen > ub->endpointLen ? -1 : 1;
    }
    return memcmp(ua->endpoint, ub->endpoint, ua->endpointLen);
}

int run_server(Callbacks cb)
{
    int sock;
    fd_set readfds;
    struct timeval tv;
    int result;
    int argc = 0;
    char **argv = NULL;
    int i;
    connection_t *connList = NULL;
    int addressFamily = AF_INET6;
    int opt;
    const char *localPort = LWM2M_STANDARD_PORT_STR;

    lwm2m_callbacks = &cb;
    pthread_mutex_init(&lock, NULL);

    connMap = hashmap_new(sizeof(connection_t *), 0, 0, 0, conn_hash, conn_compare, NULL);
    clientByEndpointMap = hashmap_new(sizeof(endpoint_t), 0, 0, 0, endpoint_hash, endpoint_compare, NULL);

    command_desc_t commands[] =
        {
            {"list", "List registered clients.", NULL, prv_output_clients, NULL},
            {"read", "Read from a client.", " read CLIENT# URI\r\n"
                                            "   CLIENT#: client number as returned by command 'list'\r\n"
                                            "   URI: uri to read such as /3, /3/0/2, /1024/11, /1024/0/1\r\n"
                                            "Result will be displayed asynchronously.",
             prv_read_client, NULL},
            {"disc", "Discover resources of a client.", " disc CLIENT# URI\r\n"
                                                        "   CLIENT#: client number as returned by command 'list'\r\n"
                                                        "   URI: uri to discover such as /3, /3/0/2, /1024/11, /1024/0/1\r\n"
                                                        "Result will be displayed asynchronously.",
             prv_discover_client, NULL},
            {"write", "Write to a client.", " write CLIENT# URI DATA\r\n"
                                            "   CLIENT#: client number as returned by command 'list'\r\n"
                                            "   URI: uri to write to such as /3, /3/0/2, /1024/11, /1024/0/1\r\n"
                                            "   DATA: data to write. Text or a supported JSON format.\r\n"
                                            "Result will be displayed asynchronously.",
             prv_write_client, NULL},
            {"update", "Write to a client with partial update.", " update CLIENT# URI DATA\r\n"
                                                                 "   CLIENT#: client number as returned by command 'list'\r\n"
                                                                 "   URI: uri to write to such as /3, /3/0/2, /1024/11, /1024/0/1\r\n"
                                                                 "   DATA: data to write. Must be a supported JSON format.\r\n"
                                                                 "Result will be displayed asynchronously.",
             prv_update_client, NULL},
            {"time", "Write time-related attributes to a client.", " time CLIENT# URI PMIN PMAX\r\n"
                                                                   "   CLIENT#: client number as returned by command 'list'\r\n"
                                                                   "   URI: uri to write attributes to such as /3, /3/0/2, /1024/11, /1024/0/1\r\n"
                                                                   "   PMIN: Minimum period\r\n"
                                                                   "   PMAX: Maximum period\r\n"
                                                                   "Result will be displayed asynchronously.",
             prv_time_client, NULL},
            {"attr", "Write value-related attributes to a client.", " attr CLIENT# URI LT GT [STEP]\r\n"
                                                                    "   CLIENT#: client number as returned by command 'list'\r\n"
                                                                    "   URI: uri to write attributes to such as /3/0/2, /1024/0/1\r\n"
                                                                    "   LT: \"Less than\" value\r\n"
                                                                    "   GT: \"Greater than\" value\r\n"
                                                                    "   STEP: \"Step\" value\r\n"
                                                                    "Result will be displayed asynchronously.",
             prv_attr_client, NULL},
            {"clear", "Clear attributes of a client.", " clear CLIENT# URI\r\n"
                                                       "   CLIENT#: client number as returned by command 'list'\r\n"
                                                       "   URI: uri to clear attributes of such as /3, /3/0/2, /1024/11, /1024/0/1\r\n"
                                                       "Result will be displayed asynchronously.",
             prv_clear_client, NULL},
            {"exec", "Execute a client resource.", " exec CLIENT# URI\r\n"
                                                   "   CLIENT#: client number as returned by command 'list'\r\n"
                                                   "   URI: uri of the resource to execute such as /3/0/2\r\n"
                                                   "Result will be displayed asynchronously.",
             prv_exec_client, NULL},
            {"del", "Delete a client Object instance.", " del CLIENT# URI\r\n"
                                                        "   CLIENT#: client number as returned by command 'list'\r\n"
                                                        "   URI: uri of the instance to delete such as /1024/11\r\n"
                                                        "Result will be displayed asynchronously.",
             prv_delete_client, NULL},
            {"create", "Create an Object instance.", " create CLIENT# URI DATA\r\n"
                                                     "   CLIENT#: client number as returned by command 'list'\r\n"
                                                     "   URI: uri to which create the Object Instance such as /1024, /1024/45 \r\n"
                                                     "   DATA: data to initialize the new Object Instance (0-255 for object 31024 or any supported JSON format) \r\n"
                                                     "Result will be displayed asynchronously.",
             prv_create_client, NULL},
            {"observe", "Observe from a client.", " observe CLIENT# URI\r\n"
                                                  "   CLIENT#: client number as returned by command 'list'\r\n"
                                                  "   URI: uri to observe such as /3, /3/0/2, /1024/11\r\n"
                                                  "Result will be displayed asynchronously.",
             prv_observe_client, NULL},
            {"cancel", "Cancel an observe.", " cancel CLIENT# URI\r\n"
                                             "   CLIENT#: client number as returned by command 'list'\r\n"
                                             "   URI: uri on which to cancel an observe such as /3, /3/0/2, /1024/11\r\n"
                                             "Result will be displayed asynchronously.",
             prv_cancel_client, NULL},

            {"q", "Quit the server.", NULL, prv_quit, NULL},

            COMMAND_END_LIST};

    opt = 1;
    while (opt < argc)
    {
        if (argv[opt] == NULL || argv[opt][0] != '-' || argv[opt][2] != 0)
        {
            print_usage();
            return 0;
        }
        switch (argv[opt][1])
        {
        case '4':
            addressFamily = AF_INET;
            break;
        case 'l':
            opt++;
            if (opt >= argc)
            {
                print_usage();
                return 0;
            }
            localPort = argv[opt];
            break;
        default:
            print_usage();
            return 0;
        }
        opt += 1;
    }

    sock = create_socket(localPort, addressFamily);
    if (sock < 0)
    {
        fprintf(stderr, "Error opening socket: %d\r\n", errno);
        return -1;
    }

    lwm2mH = lwm2m_init(NULL);
    if (NULL == lwm2mH)
    {
        fprintf(stderr, "lwm2m_init() failed\r\n");
        return -1;
    }

    signal(SIGINT, handle_sigint);

    for (i = 0; commands[i].name != NULL; i++)
    {
        commands[i].userData = (void *)lwm2mH;
    }
    fprintf(stdout, "> ");
    fflush(stdout);

    lwm2m_set_monitoring_callback(lwm2mH, prv_monitor_callback, lwm2mH);
    lwm2m_set_aa_callback(lwm2mH, cb.aaCallback, NULL);
    lwm2m_set_registered_callback(lwm2mH, registration_callback, lwm2mH);
    lwm2m_set_connected_callback(lwm2mH, cb.connectedCallback);
    lwm2m_set_disconnected_callback(lwm2mH, cb.disconnectedCallback);

    while (0 == g_quit)
    {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        tv.tv_sec = 60;
        tv.tv_usec = 0;

        result = lwm2m_step(lwm2mH, &(tv.tv_sec));
        if (result != 0)
        {
            fprintf(stderr, "lwm2m_step() failed: 0x%X\r\n", result);
            return -1;
        }

        result = select(FD_SETSIZE, &readfds, 0, 0, &tv);

        if (result < 0)
        {
            if (errno != EINTR)
            {
                fprintf(stderr, "Error in select(): %d\r\n", errno);
            }
        }
        else if (result > 0)
        {
            uint8_t buffer[MAX_PACKET_SIZE];
            int numBytes;

            if (FD_ISSET(sock, &readfds))
            {
                struct sockaddr_storage addr;
                socklen_t addrLen;

                addrLen = sizeof(addr);
                numBytes = recvfrom(sock, buffer, MAX_PACKET_SIZE, 0, (struct sockaddr *)&addr, &addrLen);

                if (numBytes == -1)
                {
                    fprintf(stderr, "Error in recvfrom(): %d\r\n", errno);
                }
                else
                {
                    char s[INET6_ADDRSTRLEN];
                    in_port_t port;
                    connection_t **mapP;
                    connection_t *connP, *connFilterP;

                    s[0] = 0;
                    if (AF_INET == addr.ss_family)
                    {
                        struct sockaddr_in *saddr = (struct sockaddr_in *)&addr;
                        inet_ntop(saddr->sin_family, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
                        port = saddr->sin_port;
                    }
                    else if (AF_INET6 == addr.ss_family)
                    {
                        struct sockaddr_in6 *saddr = (struct sockaddr_in6 *)&addr;
                        inet_ntop(saddr->sin6_family, &saddr->sin6_addr, s, INET6_ADDRSTRLEN);
                        port = saddr->sin6_port;
                    }

                    //                    fprintf(stderr, "%d bytes received from [%s]:%hu\r\n", numBytes, s, ntohs(port));
                    //                    output_buffer(stderr, buffer, numBytes, 0);
                    connFilterP = &(connection_t){.mapKey = &addr, .addrLen = addrLen};
                    mapP = (connection_t **)hashmap_get(connMap, &connFilterP);
                    if (mapP != NULL)
                    {
                        ZF_LOGD("Connection pointer found in MAP %p>>>>>>>>>>>>>>>>>>\n", connP);
                        connP = *mapP;
                    }
                    else
                    {
                        connP = NULL;
                    }
                    if (connP != NULL)
                    {
                        ZF_LOGD("Connection found in MAP %p>>>>>>>>>>>>>>>>>>\n", connP);
                    }
                    else
                    {
                        ZF_LOGD("Connection not found in MAP, fallback to list? >>>>>>>>>>>>>>>>>>\n");
                        connP = connection_find(connList, &addr, addrLen);
                        if (connP == NULL)
                        {
                            connP = connection_new_incoming(connList, sock, (struct sockaddr *)&addr, addrLen);
                            ZF_LOGD("Connection allocated %p>>>>>>>>>>>>>>>>>>\n", connP);
                            hashmap_set(connMap, &connP);
                            if (connP != NULL)
                            {
                                connList = connP;
                            }
                        }
                    }

                    ZF_LOGD("Connection found %p>>>>>>>>>>>>>>>>>>\n", connP);
                    if (connP != NULL)
                    {
                        pthread_mutex_lock(&lock);
                        lwm2m_handle_packet(lwm2mH, buffer, numBytes, connP);
                        pthread_mutex_unlock(&lock);
                    }
                }
            }
            else if (FD_ISSET(STDIN_FILENO, &readfds))
            {
                numBytes = read(STDIN_FILENO, buffer, MAX_PACKET_SIZE - 1);

                if (numBytes > 1)
                {
                    buffer[numBytes] = 0;
                    pthread_mutex_lock(&lock);
                    handle_command(commands, (char *)buffer);
                    pthread_mutex_unlock(&lock);
                    fprintf(stdout, "\r\n");
                }
                if (g_quit == 0)
                {
                    fprintf(stdout, "> ");
                    fflush(stdout);
                }
                else
                {
                    fprintf(stdout, "\r\n");
                }
            }
        }
    }

    lwm2m_close(lwm2mH);
    close(sock);
    connection_free(connList);

#ifdef MEMORY_TRACE
    if (g_quit == 1)
    {
        trace_print(0, 1);
    }
#endif

    hashmap_free(connMap);
    return 0;
}

typedef struct
{
    void *user_data;
    ReadCallback cb;
} ReadCallbackData;

static void prv_read_callback(uint16_t clientID,
                              lwm2m_uri_t *uriP,
                              int status,
                              lwm2m_media_type_t format,
                              uint8_t *data,
                              int dataLength,
                              void *userData)
{
    ZF_LOGD("Read completed: status - %d, data l - %d\n", status, dataLength);
    if (lwm2m_callbacks != NULL && lwm2m_callbacks->readCallback != NULL)
    {
        ZF_LOGD("Execute read callback: status - %d, data l - %d, ud: %p, cb: %p\n",
                status, dataLength, userData, lwm2m_callbacks->readCallback);
        lwm2m_callbacks->readCallback(status, data, dataLength, userData);
    }
}

int iotts_lwm2m_read(char *endpoint,
                     int32_t objId,
                     int32_t instId,
                     int32_t resId,
                     int timeout,
                     void *user_data)
{
    uint16_t clientId;
    lwm2m_uri_t uri;
    char *end = NULL;
    endpoint_t *endpointP = NULL;
    int result;
    ReadCallbackData *callbackDataP = NULL;

    memset(&uri, 0xFF, sizeof(lwm2m_uri_t));
    uri.objectId = objId;
    if (instId != -1)
        uri.instanceId = instId;

    if (resId != -1)
        uri.resourceId = resId;

    pthread_mutex_lock(&lock);
    // lookup clientId by endpoint
    endpointP = (endpoint_t *)hashmap_get(
        clientByEndpointMap, &(endpoint_t){
                                 .endpoint = endpoint,
                                 .endpointLen = strlen(endpoint) // optimize by keep len outside and pass?
                             });
    if (endpointP == NULL)
    {
        ZF_LOGD("Can not find client by endpoint: %s\n", endpoint);
        pthread_mutex_unlock(&lock);
        return 1;
    }
    ZF_LOGD("Client found by endpoint: %s => id: %d", endpoint, endpointP->clientId);
    result = lwm2m_dm_read(lwm2mH, endpointP->clientId, &uri, prv_read_callback, user_data);
    ZF_LOGD("Read requested: %d/%d/%d status: %d\n", uri.objectId, uri.instanceId, uri.resourceId, result);
    pthread_mutex_unlock(&lock);

    if (result == 0)
    {
        fprintf(stdout, "OK\n");
    }
    else
    {
        prv_print_error(result);
        return result;
    }

    return 0;
}