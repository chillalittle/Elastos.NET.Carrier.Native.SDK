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
 * Carrier filetransfer state.
 * The filetransfer state will be changed according to the phase of filetransfer.
 */
typedef enum ElaFileTransferState {
    /** The filetransfer handler is opened */
    ElaFileTransferState_opened = 1,
    /** The filetransfer instance is ready to pull file data or send file data. */
    ElaFileTransferState_ready,
    /** TODO */
    ElaFileTransferState_connecting,
    /** TODO */
    ElaFileTransferState_connected,
    /** TODO */
    ElaFileTransferState_closed,
    /** TODO */
    ElaFileTransferState_failed
} ElaFileTransferState;

/**
 * \~English
 * Carrier filetransfer callacks.
 */
typedef struct ElaFileTransferCallbacks {
    /**
     * \~English
     * An application-defined function that perform the state changed event.
     *
     * @param
     *      filetransfer    [in] A handle to file transfer instance.
     * @param
     *      state           [in] The state of file transfer instance.
     * @param
     *      context         [in] The application defined context data.
     *
     */
    void (*state_changed)(ElaFileTransfer *filetransfer,
                          ElaFileTransferState state, void *context);

    /**
     * \~English
     * An application-defined function that receive file transfer pull request
     * event.
     *
     * @param
     *      filetransfer    [in] A handle to the Carrier file transfer
     *                           instance.
     * @param
     *      fileid          [in] The file identifier.
     * @param
     *      offset          [in] The start offset of data to transfer in file.
     * @param
     *      length          [in] The length of data to transfer for file.
     * @param
     *      context         [in] The application defined context data.
     *
     * @return
     *      0 on success, or -1 if an error occurred. The specific error code
     *      can be retrieved by calling ela_get_error().
     */
     void (*pull_request)(ElaFileTransfer *filetransfer, const char *fileid,
                          uint64_t offset, size_t length, void *context);
    /**
     * \~English
     * An application-defined function that perform receiving data.
     *
     * @param
     *      filetransfer    [in] A handle to file transfer instance.
     * @param
     *      fileid          [in] The unique idenfier of transfering file.
     * @param
     *      offset          [in] The start offset of transfering file.
     * @param
     *      data            [in] The pointer to received data.
     * @param
     *      length          [in] The length of received data.
     * @param
     *      context         [in] The application defined context data.
     */
    void (*data)(ElaFileTransfer *filetransfer, const char *fileid,
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
    void (*pending)(ElaFileTransfer *file_transfer, const char *fileid,
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
    void (*resume)(ElaFileTransfer *file_transfer, const char *fileid,
                    void *context);

} ElaFileTransferCallbacks;

/**
 * \~English
 * An application-defined function that receive file transfer request
 * complete event.
 *
 * ElaSessionRequestCompleteCallback is the callback function type.
 *
 * @param
 *      carrier         [in] A handle to the Carrier node instance.
 * @param
 *      address         [in] The target address.
 * @param
 *      fileinfo_list   [in] Array of file informatios.
 * @param
 *      count           [in] The count of file informations.
 * @param
 *      context         [in] The application defined context data.
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
 *      carrier         [in] A handle to the Carrier node instance.
 * @param
 *      callback        [in] A pointer to file transfer request callback.
 * @param
 *      context         [in] The application defined context data.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
CARRIER_API
int ela_filetransfer_init(ElaCarrier *carrier,
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
 */
CARRIER_API
void ela_filetransfer_cleanup(ElaCarrier *carrier);

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
 *      carrier         [in] A handle to the Carrier node instance.
 * @param
 *      address         [in] The target address.
 * @param
 *      fileinfo_list   [in] Array of file informatios.
 * @param
 *      count           [in] The count of file informations.
 * @param
 *      callbacks       [in] A pointer to ElaFileTransferCallbacks to handle
 *                           all events related to new filetransfer instance.
 * @param
 *      context         [in] The application defined context data.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
CARRIER_API
ElaFileTransfer *ela_filetransfer_open(ElaCarrier *carrier,
                                       const char *address,
                                       ElaFileTransferInfo *fileinfo_list,
                                       size_t count,
                                       ElaFileTransferCallbacks *callbacks,
                                       void *context);

/**
 * \~English
 * Close file transfer instance.
 *
 * @param
 *      filetransfer    [in] A handle to the Carrier file transfer instance.
 */
CARRIER_API
void ela_filetransfer_close(ElaFileTransfer *filetransfer);

/**
 * \~English
 * Get an unique file identifier of specified file.
 *
 * Each file has it's unique fileid used between two peers.
 *
 * @param
 *      filetransfer    [in] A handle to the Carrier file transfer instance.
 * @param
 *      filename        [in] The target file name.
 * @param
 *      fileid          [in] A buffer to receive file identifier.
 * @param
 *      length          [in] The length of buffer to receive file identifier.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
CARRIER_API
char *ela_file_transfer_get_fileid(ElaFileTransfer *filetransfer,
                                   const char *filename,
                                   char *fileid, size_t length);

/**
 * \~English
 * Carrier file transfer request complete callbacks
 *
 * @param
 *      filetransfer    [in] A handle to the Carrier file transfer instance.
 * @param
 *      status          [in] The status code of the response. 0 is success,
 *                          otherwise is error.
 * @param
 *      reason          [in] The error message if status is error, or NULL
 *                          if success.
 * @param
 *      context         [in] The application defined context data.
 */

typedef void (*ElaFileTransferRequestCompleteCallback)(ElaFileTransfer *filetransfer,
                  int status, const char *reason, void *context);

/**
 * \~English
 * Send a file transfer request to target peer.
 *
 * @param
 *      filetransfer    [in] A handle to the Carrier file transfer instance.
 * @param
 *      callback        [in] A pointer to ElaFileTransferRequestCompleteCallback
 *                           function to receive the file transfer response.
 * @param
 *      context         [in] The application defined context data.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
CARRIER_API
int ela_filetransfer_request(ElaFileTransfer *filetransfer,
                             ElaFileTransferRequestCompleteCallback *callback,
                             void *context);

/**
 * \~English
 * Send a file transfer response to target peer to reply file transfer
 * request.
 *
 * @param
 *      filetransfer    [in] A handle to the Carrier file transfer instance.
 * @param
 *      status          [in] The status code of the response. 0 is success,
 *                           otherwise is error.
 * @param
 *      reason          [in] The error message if status is error, or NULL
 *                           if success.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
CARRIER_API
int ela_filetransfer_response(ElaFileTransfer *filetransfer, int status,
                              const char *reason);

/**
 * \~English
 * To send pull request to transfer file with specified file.
 *
 * @param
 *      filetransfer    [in] A handle to the Carrier file transfer instance.
 * @param
 *      fileid          [in] The file identifier.
 * @param
 *      offset          [in] The start offset of data to transfer in file.
 * @param
 *      length          [in] The length of data to transfer for file.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
CARRIER_API
int ela_filetransfer_pull(ElaFileTransfer *filetransfer, const char *fileid,
                          uint64_t offset, size_t length);

/**
 * \~English
 * To transfer file data with specified file.
 *
 * @param
 *      filetransfer    [in] A handle to the Carrier file transfer instance.
 * @param
 *      fileid          [in] The file identifier.
 * @param
 *      offset          [in] The start offset of data to transfer in file.
 * @param
 *      data            [in] The data to transfer for file.
 * @param
 *      length          [in] The length of data to transfer for file.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
CARRIER_API
int ela_filetransfer_send(ElaFileTransfer  *filetransfer, const char *fileid,
                          uint64_t offset, const uint8_t *data, size_t length);

/**
 * \~English
 * Pause transfering file data with specified file.
 *
 * @param
 *      filetransfer    [in] A handle to the Carrier file transfer instance.
 * @param
 *      fileid          [in] The file identifier.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
CARRIER_API
int ela_filetransfer_pause(ElaFileTransfer *filetransfer, const char *fileid);

/**
 * \~English
 * Resume transfering file data with specified file.
 *
 * @param
 *      filetransfer    [in] A handle to the Carrier file transfer instance.
 * @param
 *      fileid          [in] The file identifier.
 *
 * @return
 *      0 on success, or -1 if an error occurred. The specific error code
 *      can be retrieved by calling ela_get_error().
 */
CARRIER_API
int ela_filetransfer_resume(ElaFileTransfer *filetransfer, const char *fileid);

#ifdef __cplusplus
}
#endif

#if defined(__APPLE__)
#pragma GCC diagnostic pop
#endif

#endif /* __ELA_FILETRANSFER_H__ */
