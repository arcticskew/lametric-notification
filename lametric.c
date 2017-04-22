/*
 * LaMetric notification sender
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <stdbool.h>
#include <curl/curl.h>
#include "nxjson/nxjson.h"

#define DEFAULT_HOST      "192.168.0.1"
#define DEFAULT_ICON      "i2583"
#define DEFAULT_PRIORITY  "info"
#define DEFAULT_KEY       ""

static bool noop = false;
static bool verbose = false;

struct MemoryStruct {
    char *memory;
    size_t size;
};

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    mem->memory = realloc(mem->memory, mem->size + realsize + 1);
    if(mem->memory == NULL) {
        /* out of memory! */ 
        printf("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

int msg_to_lametric(char url[], char key[], char json[]) {
    CURL *curl;
    CURLcode res;
    struct curl_slist *list = NULL;

    char userpwd[75];
    strcpy(userpwd, "dev:");
    strcat(userpwd, key);
    printf("userpwd: %s\n", userpwd);

    // set up memory structure to receive response
    struct MemoryStruct data;
    data.memory = malloc(1);
    data.size = 0;

    if (verbose) printf("Posting to %s\n", url);

    // init curl
    curl = curl_easy_init();

    // send message
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url);

        // headers
        list = curl_slist_append(list, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, list);

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json);
        curl_easy_setopt(curl, CURLOPT_USERPWD, userpwd);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&data);
        if (!verbose) curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

        // post
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl failed: %s\n", curl_easy_strerror(res));
        } else {
            if (verbose) printf("%lu bytes retrieved\n", (long)data.size);
            if (verbose) printf("Data: %.*s\n", (int)data.size, data.memory);

            const nx_json* json;

            char jsonstr[500];
            strncpy(jsonstr, data.memory, 500);

            // parse returned JSON data
            json = nx_json_parse(data.memory, 0);
            if (json == NULL) {
                printf("Error decoding json: \"%s\"\n", jsonstr);
                nx_json_free(json);
                return 0;
            }

            // get message ID from success JSON
            const nx_json* id;
            char *eptr;
            id = nx_json_get(nx_json_get(json, "success"), "id");
            if (id->type == NX_JSON_STRING) {
                return strtol(id->text_value, &eptr, 10);
            } else {
                printf("Something went wrong.\n");
                return -1;
            }
        }

        curl_slist_free_all(list); /* free the list again */
        curl_easy_cleanup(curl);
        free(data.memory);
    }

    return 0;
}

int main(int argc, char *argv[]) {
    // req parameters
    char host[20];
    char key[70];
    char priority[10];
    char icon[10];
    char *message = NULL;
    
    // optional parameters
    char *sound = NULL;

    char *envhost;
    char *envkey;

    int res;
    int c;

    // set defaults
    envhost = getenv("LAMETRIC_HOST");
    if (envhost != NULL) {
        strcpy(host, envhost);
    } else {
        strcpy(host, DEFAULT_HOST);
    }
    envkey = getenv("LAMETRIC_KEY");
    if (envkey != NULL) {
        strcpy(key, envkey);
    } else {
        strcpy(key, DEFAULT_KEY);
    }
    strcpy(priority, DEFAULT_PRIORITY);
    strcpy(icon, DEFAULT_ICON);

    static struct option long_options[] = {
        { "priority",   required_argument, 0, 'p' },
        { "icon",       required_argument, 0, 'i' },
        { "sound",      required_argument, 0, 's' },
        { "host",       required_argument, 0, 'h' },
        { "key",        required_argument, 0, 'k' },
        { "noop",       no_argument,       0, 'n' },
        { "verbose",    no_argument,       0, 'v' },
        { 0, 0, 0, 0}
    };

    while ((c = getopt_long(argc, argv, "p:i:s:h:nv", long_options, NULL)) != -1) {
        switch (c) {
            case 'p':
                strncpy(priority, optarg, sizeof(priority));
                break;
            case 'i':
                strncpy(icon, optarg, sizeof(icon));
                break;
            case 's':
                sound = strdup(optarg);
                break;
            case 'h':
                strncpy(host, optarg, sizeof(host));
                break;
            case 'k':
                strncpy(key, optarg, sizeof(key));
                break;
            case 'n':
                noop = true;
                break;
            case 'v':
                verbose = true;
                break;
            case '?':
                printf("hmmm\n");
                break;
            default:
                fprintf(stderr, "Usage: %s [-p priority] [-i icon] [-s sound] [-h host] message\n", argv[0]);
                return 0;
        }
    }

    if (optind < argc) {
        message = strdup(argv[optind++]);
    } else {
        fprintf(stderr, "Usage: %s [-p priority] [-i icon] [-s sound] [-h host] message\n", argv[0]);
        return(1);
    }

    // compose URL
    char url[100];
    strcpy(url, "http://");
    strcat(url, host);
    strcat(url, ":8080/api/v2/device/notifications");

    // compose JSON
    char json[500];
    strcpy(json, "{\"priority\": \"");
    strcat(json, priority);
    strcat(json, "\", \"icon_type\": \"none\", \"model\": {\"frames\": [");

    // message
    strcat(json, "{\"text\": \"");
    strcat(json, message);
    strcat(json, "\", \"icon\": \"");
    strcat(json, icon);
    strcat(json, "\"}");

    strcat(json, "], \"cycles\": 1");

    // sound
    if (sound) {
        strcat(json, ", \"sound\": {\"category\": \"notifications\", \"id\": \"");
        strcat(json, sound);
        strcat(json, "\", \"repeat\": 1}");
    }

    strcat(json, "}}");

    if (verbose) printf("URL: %s\n", url);
    if (verbose) printf("Access key: %s\n", key);
    if (verbose) printf("JSON: %s\n", json);

    // send to LaMetric
    if (!noop) {
        res = msg_to_lametric(url, key, json);
        if (res > 0) { 
            if (verbose) printf("Message ID: %d\n", res);
        }
    }
}
