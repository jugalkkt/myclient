#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define PORT 4242
#define BUFFER_SIZE 1024
#define SEND_CHUNK_SIZE 512  // Send in chunks to handle large file
#define READ_CHUNK_SIZE 256
char read_buffer[READ_CHUNK_SIZE];

/*#define FS_O_CREATE 0x10
//#define FS_O_WRITE 0x02*/

#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, LOG_LEVEL_DBG);

#include <zephyr/net/ethernet.h>


/*int send_complete_buffer(int sockfd, const void *buffer, size_t length) {
    const char *data = (const char *)buffer;
    size_t total_sent = 0;
    ssize_t sent;
    
    while (total_sent < length) {
        size_t chunk_size = (length - total_sent < SEND_CHUNK_SIZE) ? 
                           (length - total_sent) : SEND_CHUNK_SIZE;
        
        sent = send(sockfd, data + total_sent, chunk_size, 0);
        if (sent < 0) {
            LOG_ERR("Send failed: %d", errno);
            return -1;
        }
        
        total_sent += sent;
        LOG_INF("Sent chunk: %d bytes, Total: %zu/%zu", sent, total_sent, length);
        
        // Small delay to prevent overwhelming the receiver
        k_msleep(10);
    }
    
    return 0;
}*/

void main() {
    if (!device_is_ready(FIXED_PARTITION_DEVICE(storage_partition))) {
    	LOG_ERR("Storage partition device not ready");
    	return -ENODEV;
    }

    // code for mounting lfs
    FS_LITTLEFS_DECLARE_DEFAULT_CONFIG(storage_fs_data);
    struct fs_mount_t littlefs_mnt = {
    .type = FS_LITTLEFS,
    .fs_data = &storage_fs_data,
    .storage_dev = (void *)FIXED_PARTITION_ID(storage_partition),
    .mnt_point = "/lfs"
    };
    int rc = fs_mount(&littlefs_mnt);
    if (rc < 0) {
       printk("Failed to mount LittleFS: %d\n", rc);
    }
    else {
    	printf("succesfully mounted via c code with api functions\n");
    }



    int sockfd, new_sock;
    struct sockaddr_in servaddr, cliaddr;
    //char buffer[BUFFER_SIZE];

    // Create TCP socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation error");
        exit(1);
    }
    printf("Hello\n");
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("192.168.1.30"); // or INADDR_ANY
    servaddr.sin_port = htons(PORT);

    // connect to server using connect() function
    int e;
    e = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if(e == -1) {
        perror("[-]Error in socket");
        exit(1);
    }

    printf("Client connected!");

    // OPEN FILE and save to buffer
    
    //code for making buffer
    size_t buf_size = 11000;
    char *buffer = malloc(buf_size);
    if (!buffer) {
        perror("malloc failed");
        return 1;
    }

    // code for listing files in mounted directory
    /*char * path = "/lfs";
    struct fs_dir_t dirp;
    fs_dir_t_init(&dirp);
    int res;
    static struct fs_dirent entry;
    rc = fs_opendir(&dirp, path);
    if (rc){
    	LOG_ERR("error opening directory\n");
    }
    LOG_PRINTK("\nListing dir %s ...\n", path);
        for (;;) {
                 
                res = fs_readdir(&dirp, &entry);

                if (res || entry.name[0] == 0) {
                        if (res < 0) {
                                LOG_ERR("Error reading dir [%d]\n", res);
                        }
                        break;
                }

                if (entry.type == FS_DIR_ENTRY_DIR) {
                        LOG_PRINTK("[DIR ] %s\n", entry.name);
                } else {
                        LOG_PRINTK("[FILE] %s (size = %zu)\n",
                                   entry.name, entry.size);
                }
        }*/

    //recv(new_sock, &filename_len, sizeof(filename_len), 0);

    // Receive file size
    int file_size;
    recv(sockfd, &file_size, sizeof(file_size), 0);
    printf("File size is %d bytes\n", file_size);
    
    // Open file for writing
    char *fname = "/lfs/text.llext";
    struct fs_file_t file;
    fs_file_t_init(&file);
    printf("file name you gave is %s\n", fname);
    rc = fs_open(&file, fname,FS_O_CREATE | FS_O_WRITE);
    if (rc < 0) {
                LOG_ERR("lFAIL: open %s: %d", fname, rc);
                return rc;
    }

    // Receive file data
    long bytes_received = 0;
    while (bytes_received < file_size) {
        int chunk_size = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (chunk_size <= 0) break;
        rc = fs_write(&file, buffer, file_size); // &buffer
    	if (rc < 0) {
                printf("FAIL: read %s: [rd:%d]", fname, rc);
        }
        bytes_received += chunk_size;
    }
    fs_close(&file);
    printf("file recieved succcessfully...\n");

    // Read data from file to buffer then print it 
    printf("\n=== Reading and printing LLEXT file contents ===\n");

    // Re-open the file for reading
    struct fs_file_t read_file;
    fs_file_t_init(&read_file);
    rc = fs_open(&read_file, fname, FS_O_READ);
    if (rc < 0) {
    	LOG_ERR("Failed to open %s for reading: %d", fname, rc);
    	free(buffer);
    	close(sockfd);
    	return;
    }  

    // Read and print file contents in chunks
    int total_read = 0;
    int chunk_num = 0;

    while (total_read < file_size) {
    // Read a chunk from the file
       ssize_t bytes_read = fs_read(&read_file, read_buffer, READ_CHUNK_SIZE);
        if (bytes_read < 0) {
            LOG_ERR("Failed to read file: %d", bytes_read);
            break;
        }
    
        if (bytes_read == 0) {
            printf("End of file reached\n");
            break;
        }
    
    total_read += bytes_read;
    chunk_num++;
    
    printf("\n--- Chunk %d: %d bytes ---\n", chunk_num, bytes_read);
    
    LOG_HEXDUMP_INF(read_buffer, bytes_read, "LLEXT file data:");
    }


   
   
   printf("\nTotal bytes read: %d/%d\n", total_read, file_size);
   fs_close(&read_file);
    
    

    // code for getting file_size using fs_stat
    /*struct fs_dirent file_stat;
    rc = fs_stat(fname, &file_stat);
    if (rc < 0) {
        LOG_ERR("Failed to get file stats: %d", rc);
        return;
    }
    int file_size = file_stat.size;
    LOG_INF("File size is  %d bytes", file_size);
    

    // get file metadata (filename and size)
    const char *send_filename = "recevd_file.llext";
    int filename_len = strlen(send_filename);
    
    // Send filename length
    if (send(sockfd, &filename_len, sizeof(filename_len), 0) < 0) {
        LOG_ERR("Failed to send filename length\n");
    }
    else LOG_INF("Succesfully sent filename length\n");
    
    // Send filename
    if (send(sockfd, send_filename, filename_len, 0) < 0) {
        LOG_ERR("Failed to send filename\n");
    }
    else LOG_INF("Succesfully sent filename\n");
    
    // Send file size
    if (send(sockfd, &file_size, sizeof(file_size), 0) < 0) {
        LOG_ERR("Failed to send file size");
    } 
    else LOG_INF("Succesfully sent file size ie %d\n", file_size);
    
    // read file content into file_buffer
    char *file_buffer = k_malloc(file_size);
    ssize_t bytes_read = fs_read(&file, file_buffer, file_size);

    // Send complete file buffer
    if (send_complete_buffer(sockfd, file_buffer, file_size) == 0) {
        LOG_INF("File sent successfully!");
    } else {
        LOG_ERR("Failed to send file buffer");
    }*/
    
    free(buffer);
    close(sockfd);
}



