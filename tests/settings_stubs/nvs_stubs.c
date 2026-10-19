#include "nvs.h"
#include "nvs_flash.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    char key[16];
    unsigned char data[256];
    size_t length;
    bool used;
} stub_entry_t;

static stub_entry_t s_entries[4];

static stub_entry_t *find_entry(const char *key, bool create)
{
    stub_entry_t *free_entry = NULL;
    for (size_t i = 0; i < sizeof(s_entries) / sizeof(s_entries[0]); i++) {
        if (s_entries[i].used && strcmp(s_entries[i].key, key) == 0) {
            return &s_entries[i];
        }
        if (!s_entries[i].used && !free_entry) free_entry = &s_entries[i];
    }
    if (!create || !free_entry) return NULL;
    free_entry->used = true;
    free_entry->length = 0;
    snprintf(free_entry->key, sizeof(free_entry->key), "%s", key);
    return free_entry;
}

esp_err_t nvs_flash_init(void)
{
    return ESP_OK;
}

esp_err_t nvs_flash_erase(void)
{
    memset(s_entries, 0, sizeof(s_entries));
    return ESP_OK;
}

esp_err_t nvs_open(const char *name, nvs_open_mode_t open_mode,
                   nvs_handle_t *out_handle)
{
    (void)name;
    (void)open_mode;
    if (out_handle) *out_handle = 1;
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value,
                       size_t *length)
{
    (void)handle;
    if (!key || !length) return ESP_ERR_INVALID_ARG;
    stub_entry_t *entry = find_entry(key, false);
    if (!entry) return ESP_ERR_NVS_NOT_FOUND;
    if (!out_value) {
        *length = entry->length;
        return ESP_OK;
    }
    if (*length < entry->length) return ESP_ERR_NVS_INVALID_LENGTH;
    memcpy(out_value, entry->data, entry->length);
    *length = entry->length;
    return ESP_OK;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key,
                       const void *value, size_t length)
{
    (void)handle;
    if (!key || (!value && length)) return ESP_ERR_INVALID_ARG;
    if (length > sizeof(((stub_entry_t *)0)->data)) {
        return ESP_ERR_NVS_INVALID_LENGTH;
    }
    stub_entry_t *entry = find_entry(key, true);
    if (!entry) return ESP_ERR_NVS_NO_FREE_PAGES;
    memcpy(entry->data, value, length);
    entry->length = length;
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle)
{
    (void)handle;
}

