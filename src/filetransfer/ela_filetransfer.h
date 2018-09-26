/*
 * Copyright (c) 2018 Elastos Foundation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __ELA_FILETRANSFER_H__
#define __ELA_FILETRANSFER_H__

#include <stdbool.h>
#include <stdint.h>
#include <ela_carrier.h>

#if defined(__APPLE__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdocumentation"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if defined(CARRIER_STATIC)
#define CARRIER_API
#elif defined(CARRIER_DYNAMIC)
  #ifdef CARRIER_BUILD
    #if defined(_WIN32) || defined(_WIN64)
      #define CARRIER_API        __declspec(dllexport)
    #else
      #define CARRIER_API        __attribute__((visibility("default")))
    #endif
  #else
    #if defined(_WIN32) || defined(_WIN64)
      #define CARRIER_API        __declspec(dllimport)
    #else
      #define CARRIER_API        __attribute__((visibility("default")))
    #endif
  #endif
#else
#define CARRIER_API
#endif

#define ELA_MAX_FILE_NAME_LEN 1023
#define ELA_MAX_FILE_ID_LEN   45

/**
 * \~English
 * A defined type representing the transaction of file transfer.
 */
typedef struct ElaFileTransfer ElaFileTransfer;

/**
 * \~English
 * A structure representing the file transfer meta information.
 *
 * Two peer carrier node use this structure to communicate of what file
 * or file list to transfer.
 */
typedef struct ElaFileTransferInfo {
    /**
     * \~English
     * File name of transfer file, without file path.
     */
    char filename[ELA_MAX_FILE_NAME_LEN + 1];

    /**
     * \~English
     * Unique fileid of transfer file, which is being unique to current
     * node and remote friend node.
     */
    char fileid[ELA_MAX_FILE_ID_LEN + 1];

    /**
     * \~English
     * Total file size of transfer file.
     */
    size_t file_size;

    /**
     * \~English
     * Total CRC checksum of file data.
     */
    uint64_t checksum; //TODO.
} ElaFileTransferInfo;

/**
 * \~English
 * An application-defined function that receive file transfer request
 * complete event.
 *
 * ElaSessionRequestCompleteCallback is the callback function type.
 *
 * @param
 *      carrier     [in] A handle to the Carrier node instance.
 * @param
 *      address     [in] The target address.
 * @param
 *      fileinfo_list   [in] Array of file informatios.
 * @param
 *      count       [in] The count of file informations.
 * @param
 *      context     [in] The application defined context data.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
typedef void (*ElaFileTransferRequestCallback)(ElaCarrier *carrier,
                const char *address,
                ElaFileTransferInfo *fileinfo_list, size_t count,
                void *context);

/**
 * \~English
 * Initialize file transfer extension.
 *
 * The application must initialize the file transfer extension before
 * calling any file transfer API.
 *
 * @param
 *      carrier     [in] A handle to the Carrier node instance.
 * @param
 *      callback    [in] A pointer to file transfer request callback.
 * @param
 *      context     [in] The application defined context data.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
CARRIER_API
int ela_file_transfer_init(ElaCarrier *carrier,
                           ElaFileTransferRequestCallback *callback,
                           void *context);

/**
 * \~English
 * Clean up Carrier file transfer extension.
 *
 * The application should call ela_file_transfer_cleanup before quit,
 * to clean up the resources associated with the extension.
 *
 * If the extension is not initialized, this function has no effect.
 *
 * @param
 *      carrier     [in] A handle to the carrier node instance.
 */
CARRIER_API
void ela_file_transfer_cleanup(ElaCarrier *carrier);

/**
 * \~English
 * Open a file transfer instance.
 *
 * The application must open file transfer instance before sending
 * request/reply to transfer file.
 *
 * As to send request to transfer file, application should feed file
 * informations that want to transfer. And for receiving side, application
 * should feed file information received from request callback.
 *
 * @param
 *      carrier     [in] A handle to the Carrier node instance.
 * @param
 *      address     [in] The target address.
 * @param
 *      fileinfo_list   [in] Array of file informatios.
 * @param
 *      count       [in] The count of file informations.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
ElaFileTransfer *ela_file_transfer_open(ElaCarrier *carrier,
                           const char *address,
                           ElaFileTransferInfo *fileinfo_list,
                           size_t count);

/**
 * \~English
 * Close file transfer instance.
 *
 * @param
 *      file_transfer   [in] A handle to the Carrier file transfer instance.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
int ela_file_transfer_close(ElaFileTransfer *file_transfer);

/**
 * \~English
 * Get an unique file identifier of specified file.
 *
 * Each file has it's unique fileid used between two peers.
 *
 * @param
 *      file_transfer   [in] A handle to the Carrier file transfer instance.
 * @param
 *      filename    [in] The target file name.
 * @param
 *      fileid      [in] A buffer to receive file identifier.
 * @param
 *      length      [in] The length of buffer to receive file identifier.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
char *ela_file_transfer_get_fileid(ElaFileTransfer *file_transfer,
                           const char *filename,
                           char *fileid_buf,
                           size_t length);

/**
 * \~English
 * Carrier file transfer request complete callbacks.
 */
typedef struct ElaFileTransferRequestCompleteCallbacks {
    /**
     * \~English
     * An application-defined function that receive file transfer response
     * event.
     *
     * @param
     *      file_transfer   [in] A handle to the Carrier file transfer
     *                           instance.
     * @param
     *      status      [in] The status code of the response.
     *                       0 is success, otherwise is error.
     * @param
     *      reason      [in] The error message if status is error, or NULL
     *                       if success.
     * @param
     *      context     [in] The application defined context data.
     */
    void (*response)(ElaFileTransfer *file_transfer, int status,
                     const char *reason, void *context);

    /**
     * \~English
     * An application-defined function that receive file transfer pull request
     * event.
     *
     * @param
     *      file_transfer   [in] A handle to the Carrier file transfer
     *                           instance.
     * @param
     *      fileid      [in] The file identifier.
     * @param
     *      offset      [in] The start offset of data to transfer in file.
     * @param
     *      length      [in] The length of data to transfer for file.
     * @param
     *      context     [in] The application defined context data.
     *
     * @return
     *      0 on success, or -1 if an error occurred. The specific error code
     *      can be retrieved by calling ela_get_error().
     */
     void (*pull_request)(ElaFileTransfer *file_transfer, const char *fileid,
                          uint64_t offset, size_t length, void *context);

}  ElaFileTransferRequestCompleteCallbacks;

/**
 * \~English
 * Send a file transfer request to target peer.
 *
 * @param
 *      file_transfer   [in] A handle to the Carrier file transfer instance.
 * @param
 *      callbacks   [in] A pointer to ElaFileTransferRequestCompleteCallbacks
 *                       function to receive the file transfer response.
 * @param
 *      context     [in] The application defined context data.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
int ela_file_transfer_request(ElaFileTransfer *file_transfer,
                           ElaFileTransferRequestCompleteCallbacks *callbacks,
                           void *context);

/**
 * \~English
 * Send a file transfer response to target peer to reply file transfer
 * request.
 *
 * @param
 *      file_transfer   [in] A handle to the Carrier file transfer instance.
 * @param
 *      status      [in] The status code of the response.
 *                       0 is success, otherwise is error.
 * @param
 *      reason      [in] The error message if status is error, or NULL
 *                       if success.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
int ela_file_transfer_response(ElaFileTransfer *file_transfer,
                               int status,
                               const char *reason);

/**
 * \~English
 * Carrier file transfer pull callacks.
 */
typedef struct ElaFileTransferPullCallbacks {
    /**
     * \~English
     * An application-defined function that perform receiving data.
     *
     * @param
     *      file_transfer   [in] A handle to file transfer instance.
     * @param
     *      fileid          [in] The unique idenfier of transfering file.
     * @param
     *      offset          [in] The start offset of transfering file.
     * @param
     *      data            [in] The pointer to received data.
     * @param
     *      length          [in] The length of received data.
     * @param
     *      context     [in] The application defined context data.
     */
    void (*data)(ElaFileTransfer *file_transfer, const char *fileid,
                 uint64_t offset, const uint8_t *data, size_t length,
                 void *context);
    /**
     * \~English
     * An application-defined function that perform paused control packet
     * to pause file transfer.
     *
     * @param
     *      file_transfer   [in] A handle to file transfer instance.
     * @param
     *      fileid          [in] The unique idenfier of transfering file.
     * @param
     *      context         [in] The application defined context data.
     */
    void (*paused)(ElaFileTransfer *file_transfer, const char *fileid,
                   void *context);

    /**
     * \~English
     * An application-defined function that perform resume control packet
     * to resume file transfer.
     *
     * @param
     *      file_transfer   [in] A handle to file transfer instance.
     * @param
     *      fileid          [in] The unique idenfier of transfering file.
     * @param
     *      context         [in] The application defined context data.
     */
    void (*resumed)(ElaFileTransfer *file_transfer, const char *fileid,
                    void *context);

} ElaFileTransferPullCallbacks;

/**
 * \~English
 * To send pull request to transfer file with specified file.
 *
 * @param
 *      file_transfer   [in] A handle to the Carrier file transfer instance.
 * @param
 *      fileid      [in] The file identifier.
 * @param
 *      offset      [in] The start offset of data to transfer in file.
 * @param
 *      length      [in] The length of data to transfer for file.
 * @param
 *      callbacks   [in] A pointer to ElaFileTransferPullCallbacks
 *                       function to receive the file transfer data.
 * @param
 *      context     [in] The application defined context data.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
int ela_file_transfer_pull(ElaFileTransfer *file_transfer,
                           const char *fileid,
                           uint64_t offset,
                           size_t length,
                           ElaFileTransferPullCallbacks *callbacks,
                           void *context);

/**
 * \~English
 * To transfer file data with specified file.
 *
 * @param
 *      file_transfer   [in] A handle to the Carrier file transfer instance.
 * @param
 *      fileid      [in] The file identifier.
 * @param
 *      offset      [in] The start offset of data to transfer in file.
 * @param
 *      data        [in] The data to transfer for file.
 * @param
 *      length      [in] The length of data to transfer for file.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
int ela_file_transfer_send(ElaFileTransfer  *file_transfer,
                           const char *fileid,
                           uint64_t offset,
                           const uint8_t *data,
                           size_t length);

/**
 * \~English
 * Pause transfering file data with specified file.
 *
 * @param
 *      file_transfer   [in] A handle to the Carrier file transfer instance.
 * @param
 *      fileid      [in] The file identifier.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
int ela_file_transfer_pause(ElaFileTransfer *file_transfer,
                            const char *fileid);

/**
 * \~English
 * Resume transfering file data with specified file.
 *
 * @param
 *      file_transfer   [in] A handle to the Carrier file transfer instance.
 * @param
 *      fileid      [in] The file identifier.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
int ela_file_transfer_resume(ElaFileTransfer *file_transfer,
                             const char *fileid);

#ifdef __cplusplus
}
#endif

#if defined(__APPLE__)
#pragma GCC diagnostic pop
#endif

#endif /* __ELA_FILETRANSFER_H__ */
