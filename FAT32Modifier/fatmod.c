#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <linux/msdos_fs.h>
#include <time.h>

#define FALSE 0
#define TRUE 1

#define SECTORSIZE 512   // bytes
#define CLUSTERSIZE 1024 // bytes

unsigned int root_start_cluster;
unsigned int reserved_area_sectors;
char filename[128];
char diskname[128];

int readsector(int fd, unsigned char *buf, unsigned int snum);
int writesector(int fd, unsigned char *buf, unsigned int snum);
int readCluster(int fd, unsigned char *buf, unsigned int cnum);
int writeCluster(int fd, unsigned char *buf, unsigned int cnum);
void findRootDirectory(int fd);
void listFilesInRootDir(int fd);
void displayFileInASCII(int fd);
void displayFileInBinary(int fd);
void createFileInRootDir(int fd);
void deleteFile(int fd);
void writeData(int fd, int offset, int n, unsigned int data);
char *trim(char *s);

int main(int argc, char *argv[])
{
    int fd;

    if (argc < 2)
    {
        printf("Usage: is wrong. Run ./fatmod -h for help\n");
        exit(1);
    }

    if (strcmp(argv[1], "-h") == 0)
    {
        printf(" Usages: \n \
        fatmod -l DISKIMAGE \n \
        fatmod -r -a DISKIMAGE -a FILENAME \n \
        fatmod -r -b DISKIMAGE -b FILENAME \n \
        fatmod -c DISKIMAGE -c FILENAME \n \
        fatmod -d DISKIMAGE -d FILENAME \n \
        fatmod -w DISKIMAGE -w FILENAME OFFSET N DATA \n \
        fatmod -h\n");
        exit(1);
    }

    strcpy(diskname, argv[1]);
    fd = open(diskname, O_SYNC | O_RDWR);
    if (fd < 0)
    {
        printf("Could not open disk image\n");
        exit(1);
    }
    findRootDirectory(fd);

    if (strcmp(argv[2], "-l") == 0)
    {
        listFilesInRootDir(fd);
    }

    if (strcmp(argv[2], "-r") == 0)
    {
        strcpy(filename, argv[4]);
        // Convert filename to uppercase
        for (int i = 0; i < strlen(filename); i++)
        {
            filename[i] = toupper(filename[i]);
        }
        if (strcmp(argv[3], "-a") == 0)
        {
            displayFileInASCII(fd);
        }

        if (strcmp(argv[3], "-b") == 0)
        {
            displayFileInBinary(fd);
        }
    }

    if (strcmp(argv[2], "-c") == 0)
    {
        strcpy(filename, argv[3]);
        // Convert filename to uppercase
        for (int i = 0; i < strlen(filename); i++)
        {
            filename[i] = toupper(filename[i]);
        }
        createFileInRootDir(fd);
    }

    if (strcmp(argv[2], "-d") == 0)
    {
        strcpy(filename, argv[3]);
        // Convert filename to uppercase
        for (int i = 0; i < strlen(filename); i++)
        {
            filename[i] = toupper(filename[i]);
        }
        deleteFile(fd);
    }

    if (strcmp(argv[2], "-w") == 0)
    {
        strcpy(filename, argv[3]);
        // Convert filename to uppercase
        for (int i = 0; i < strlen(filename); i++)
        {
            filename[i] = toupper(filename[i]);
        }
        int offset = atoi(argv[4]);
        int n = atoi(argv[5]);
        int data = atoi(argv[6]);
        writeData(fd, offset, n, data);
    }

    close(fd);
}

void listFilesInRootDir(int fd)
{
    unsigned char cluster[CLUSTERSIZE];

    // Read the root directory
    readCluster(fd, cluster, root_start_cluster);

    // Parse root directory
    struct msdos_dir_entry *dep;
    dep = (struct msdos_dir_entry *)cluster;
    printf("Root Directory Entries:\n");
    dep++;
    for (int i = 0; i < 16; i++)
    {
        if (dep->name[0] != 0x00 && dep->name[0] != 0xe5)
        {
            char name[9];
            strncpy(name, dep->name, 8);
            name[8] = '\0';
            trim(name);
            char extension[4];
            strncpy(extension, dep->name + 8, 3);
            extension[3] = '\0';
            trim(extension);
            printf("%s.%s %d\n", name, extension, dep->size);
        }

        dep++;
    }
}
void displayFileInASCII(int fd)
{
    unsigned char cluster[CLUSTERSIZE];

    // Read the root directory
    readCluster(fd, cluster, root_start_cluster);

    // Find the file in the root directory
    struct msdos_dir_entry *dep;
    dep = (struct msdos_dir_entry *)cluster;
    dep++;
    int isFound = 0;
    unsigned int start_cluster;
    for (int i = 0; i < 30; i++)
    {
        char name[9];
        strncpy(name, dep->name, 8);
        name[8] = '\0';
        trim(name);
        char extension[4];
        strncpy(extension, dep->name + 8, 3);
        extension[3] = '\0';
        trim(extension);
        char wholeName[13];
        strcpy(wholeName, name);
        strcat(wholeName, ".");
        strcat(wholeName, extension);
        if (strcmp(wholeName, filename) == 0)
        {
            isFound = 1;
            start_cluster = dep->start | (dep->starthi << 16);
            break;
        }
        dep++;
    }

    // handle if not found
    if (!isFound)
    {
        printf("File not found\n");
        return;
    }

    if (start_cluster == 0)
    {
        printf("File is empty\n");
        return;
    }

    unsigned int current_cluster = start_cluster;
    unsigned int next_address = -1;

    // While FAT table integer is not 0x0fffffff, read the cluster and display the data as binary
    while (1)
    {
        // Parse the next cluster sector and offset from next_address
        int next_sector_no = current_cluster / (SECTORSIZE / 4);
        int next_sector_offset = current_cluster % (SECTORSIZE / 4);

        // Read the value in the FAT Table integer
        unsigned char fat_sector[SECTORSIZE];
        readsector(fd, fat_sector, reserved_area_sectors + next_sector_no);
        unsigned int *fat_entry = (unsigned int *)fat_sector;
        next_address = fat_entry[next_sector_offset];

        // If the next cluster is the last cluster, print the filled part and break the loop
        if (next_address == 0x0fffffff || next_address == 0)
        {
            printf("Cluster %d:\n", current_cluster + root_start_cluster - 2);
            unsigned char cluster2[CLUSTERSIZE];
            readCluster(fd, cluster2, current_cluster + root_start_cluster - 2);
            for (int i = 0; i < dep->size % CLUSTERSIZE; i++)
            {
                printf("%c", cluster2[i]);
            }
            printf("\n");
            break;
        }

        // Print the data
        printf("Cluster %d:\n", current_cluster + root_start_cluster - 2);
        unsigned char cluster2[CLUSTERSIZE];
        readCluster(fd, cluster2, current_cluster + root_start_cluster - 2);
        for (int i = 0; i < CLUSTERSIZE; i++)
        {
            printf("%c", cluster2[i]);
        }
        printf("\n");

        // Parse the next cluster sector and offset from next_address
        next_sector_no = next_address >> 7;
        next_sector_offset = next_address & 0x7f;
        current_cluster = next_sector_no * (SECTORSIZE / 4) + next_sector_offset;
    }
}

void displayFileInBinary(int fd)
{
    unsigned char cluster[CLUSTERSIZE];

    // Read the root directory
    readCluster(fd, cluster, root_start_cluster);

    // Find the file in the root directory
    struct msdos_dir_entry *dep;
    dep = (struct msdos_dir_entry *)cluster;
    dep++;
    int isFound = 0;
    unsigned int start_cluster;
    for (int i = 0; i < 30; i++)
    {
        char name[9];
        strncpy(name, dep->name, 8);
        name[8] = '\0';
        trim(name);
        char extension[4];
        strncpy(extension, dep->name + 8, 3);
        extension[3] = '\0';
        trim(extension);
        char wholeName[13];
        strcpy(wholeName, name);
        strcat(wholeName, ".");
        strcat(wholeName, extension);
        if (strcmp(wholeName, filename) == 0)
        {
            isFound = 1;
            start_cluster = dep->start | (dep->starthi << 16);
            break;
        }
        dep++;
    }

    // handle if not found
    if (!isFound)
    {
        printf("File not found\n");
        return;
    }

    if (start_cluster == 0)
    {
        printf("File is empty\n");
        return;
    }

    unsigned int current_cluster = start_cluster;
    unsigned int next_address = -1;

    int count = 0;
    while (1)
    {
        // Parse the next cluster sector and offset from next_address
        int next_sector_no = current_cluster / (SECTORSIZE / 4);
        int next_sector_offset = current_cluster % (SECTORSIZE / 4);

        // Read the value in the FAT Table integer
        unsigned char fat_sector[SECTORSIZE];
        readsector(fd, fat_sector, reserved_area_sectors + next_sector_no);
        unsigned int *fat_entry = (unsigned int *)fat_sector;
        next_address = fat_entry[next_sector_offset];

        // If the next cluster is the last cluster, print the filled part and break the loop
        if (next_address == 0x0fffffff || next_address == 0)
        {
            unsigned char cluster2[CLUSTERSIZE];
            readCluster(fd, cluster2, current_cluster + root_start_cluster - 2);
            for (int i = 0; i < (dep->size % CLUSTERSIZE) / 16 + 1; i++)
            {
                printf("%08x: ", count);
                for (int j = 0; j < 16; j++)
                {
                    printf("%02x ", cluster2[i * 16 + j]);
                }
                count += 16;
                printf("\n");
            }
            break;
        }

        // Print the data
        unsigned char cluster2[CLUSTERSIZE];
        readCluster(fd, cluster2, current_cluster + root_start_cluster - 2);
        for (int i = 0; i < CLUSTERSIZE / 16; i++)
        {
            printf("%08x: ", count);
            for (int j = 0; j < 16; j++)
            {
                printf("%02x ", cluster2[i * 16 + j]);
            }
            count += 16;
            printf("\n");
        }
        printf("\n");

        // Parse the next cluster sector and offset from next_address
        next_sector_no = next_address >> 7;
        next_sector_offset = next_address & 0x7f;
        current_cluster = next_sector_no * (SECTORSIZE / 4) + next_sector_offset;
    }
}

void createFileInRootDir(int fd)
{
    unsigned char cluster[CLUSTERSIZE];
    readCluster(fd, cluster, root_start_cluster); // Read the root directory cluster

    struct msdos_dir_entry *dep;
    dep = (struct msdos_dir_entry *)cluster;
    dep++;
    // Find the first empty entry in the root directory
    int isFound = 0;
    for (int i = 0; i < 30; i++)
    {
        if (dep->name[0] == 0x00 || dep->name[0] == 0xe5)
        {
            isFound = 1;
            break;
        }
        dep++;
    }

    // handle if not found
    if (!isFound)
    {
        printf("Root directory is full\n");
        return;
    }

    // Find the dot in the filename
    char name[9];
    char extension[4];
    int dot_index = -1; // Initialize dot index to -1
    for (int i = 0; i < strlen(filename); i++)
    {
        if (filename[i] == '.')
        {
            dot_index = i; // Store the index of the dot
            break;
        }
    }

    // Populate the empty entry with the new file
    memcpy(dep->name, filename, dot_index);
    memcpy(dep->name + 8, filename + dot_index + 1, 3);
    dep->attr = 0x20;
    dep->starthi = 0;
    dep->start = 0;
    dep->size = 0;

    // Write the updated root directory back to the disk
    writeCluster(fd, cluster, root_start_cluster);

    printf("File created\n");
}

void deleteFile(int fd)
{
    // Read the root directory cluster
    unsigned char cluster[CLUSTERSIZE];
    readCluster(fd, cluster, root_start_cluster);

    struct msdos_dir_entry *dep;
    dep = (struct msdos_dir_entry *)cluster;
    dep++;
    // Find the first empty entry in the root directory
    int isFound = 0;
    for (int i = 0; i < 30; i++)
    {
        char name[9];
        strncpy(name, dep->name, 8);
        name[8] = '\0';
        trim(name);
        char extension[4];
        strncpy(extension, dep->name + 8, 3);
        extension[3] = '\0';
        trim(extension);
        char wholeName[13];
        strcpy(wholeName, name);
        strcat(wholeName, ".");
        strcat(wholeName, extension);

        if (strcmp(wholeName, filename) == 0)
        {
            isFound = 1;

            // Deallocate from the FAT Table
            unsigned int start_cluster = dep->start | (dep->starthi << 16);
            unsigned int next_address = start_cluster;
            unsigned int next_sector_no = next_address / (SECTORSIZE / 4);
            unsigned int next_sector_offset = next_address % (SECTORSIZE / 4);
            while (next_address != 0x0fffffff && next_address != 0)
            {
                unsigned char fat_sector[SECTORSIZE];
                readsector(fd, fat_sector, reserved_area_sectors + next_sector_no);
                unsigned int *fat_entry = (unsigned int *)fat_sector;
                next_address = fat_entry[next_sector_offset];
                fat_entry[next_sector_offset] = 0;
                writesector(fd, fat_sector, reserved_area_sectors + next_sector_no);

                // Parse the next cluster sector and offset from next_address
                int next_sector_no = next_address >> 7;
                int next_sector_offset = next_address & 0x7f;
            }

            // Delete the entry
            dep->name[0] = 0xe5;
            for (int i = 1; i < 11; i++)
            {
                dep->name[i] = 0;
            }
            dep->starthi = 0;
            dep->start = 0;

            // Write the updated root directory back to the disk
            writeCluster(fd, cluster, root_start_cluster);

            printf("File deleted\n");

            break;
        }
        dep++;
    }

    // handle if not found
    if (!isFound)
    {
        printf("File not found\n");
        return;
    }
}

void writeData(int fd, int offset, int n, unsigned int data)
{
    unsigned char cluster[CLUSTERSIZE];
    // Read the root directory
    readCluster(fd, cluster, root_start_cluster);

    // Find the file in the root directory
    struct msdos_dir_entry *dep;
    dep = (struct msdos_dir_entry *)cluster;
    dep++;
    int isFound = 0;
    unsigned int start_cluster;
    for (int i = 0; i < 30; i++)
    {
        char name[9];
        strncpy(name, dep->name, 8);
        name[8] = '\0';
        trim(name);
        char extension[4];
        strncpy(extension, dep->name + 8, 3);
        extension[3] = '\0';
        trim(extension);
        char wholeName[13];
        strcpy(wholeName, name);
        strcat(wholeName, ".");
        strcat(wholeName, extension);
        if (strcmp(wholeName, filename) == 0)
        {
            isFound = 1;
            start_cluster = dep->start | (dep->starthi << 16);
            break;
        }
        dep++;
    }

    // Check if offset is larger than the file size
    if (offset > dep->size)
    {
        printf("Offset cannot be larger than the file size which is %d \n", dep->size);
        return;
    }

    // Update the size of the file
    if (n + offset > dep->size)
    {
        dep->size = n + offset;
    }
    writeCluster(fd, cluster, root_start_cluster);

    // Check if the file is empty
    if (start_cluster == 0)
    {
        int empty_cluster = -1;
        int remaining_data = n;

        // Find one empty cluster
        for (int i = 0; i < root_start_cluster - reserved_area_sectors; i++)
        {
            unsigned char fat_sector[SECTORSIZE];
            readsector(fd, fat_sector, reserved_area_sectors + i);
            unsigned int *fat_entry = (unsigned int *)fat_sector;
            for (int j = 0; j < SECTORSIZE / 4; j++)
            {
                if (fat_entry[j] == 0)
                {
                    empty_cluster = i * SECTORSIZE / 4 + j;
                    break;
                }
            }
        }

        // While there are more data to be written, find and write new clusters
        while (remaining_data > 0)
        {
            // Put start_cluster to dentry in root directory
            if (dep->start == 0)
            {
                dep->start = empty_cluster;
                writeCluster(fd, cluster, root_start_cluster);
            }

            // Put data to the empty cluster
            unsigned char data_cluster[CLUSTERSIZE] = {0};
            for (int i = offset; i < CLUSTERSIZE && 0 < remaining_data; i++)
            {
                data_cluster[i] = data;
                remaining_data--;
            }
            writeCluster(fd, data_cluster, root_start_cluster + empty_cluster - 2);
            printf("Written to cluster %d\n", root_start_cluster + empty_cluster - 2);

            offset -= CLUSTERSIZE;
            if (offset < 0)
            {
                offset = 0;
            }

            // If no more clusters needed, put 0x0fffffff to the last cluster
            if (remaining_data == 0)
            {
                int sector_no = empty_cluster / (SECTORSIZE / 4);
                int sector_offset = empty_cluster % (SECTORSIZE / 4);
                unsigned char fat_sector[SECTORSIZE];
                readsector(fd, fat_sector, reserved_area_sectors + sector_no);
                unsigned int *fat_entry = (unsigned int *)fat_sector;
                fat_entry[sector_offset] = 0x0fffffff;
                writesector(fd, fat_sector, reserved_area_sectors + sector_no);
            }
            else
            {
                // Find one more empty cluster, to put the number of the new cluster to the previous cluster
                int new_empty_cluster = -1;
                for (int i = 0; i < root_start_cluster - reserved_area_sectors; i++)
                {
                    unsigned char fat_sector[SECTORSIZE];
                    readsector(fd, fat_sector, reserved_area_sectors + i);
                    unsigned int *fat_entry = (unsigned int *)fat_sector;
                    for (int j = 0; j < SECTORSIZE / 4; j++)
                    {
                        if (fat_entry[j] == 0 && i * SECTORSIZE / 4 + j != empty_cluster)
                        {
                            new_empty_cluster = i * SECTORSIZE / 4 + j;
                            break;
                        }
                    }
                }

                // Prepare the int to contain the address of new_empty_cluster. Bits 7-31 tell which sector to read from fat. Bits 0-6 tell which of the 128 integers in that sector contain the number of the next cluster of your file
                int next_sector_no = new_empty_cluster / (SECTORSIZE / 4);
                int next_sector_offset = new_empty_cluster % (SECTORSIZE / 4);
                int next_address = (next_sector_no << 7) | next_sector_offset;

                // Put the number of the new cluster to the previous cluster.
                int sector_no = empty_cluster / (SECTORSIZE / 4);
                int sector_offset = empty_cluster % (SECTORSIZE / 4);
                unsigned char fat_sector[SECTORSIZE];
                readsector(fd, fat_sector, reserved_area_sectors + sector_no);
                unsigned int *fat_entry = (unsigned int *)fat_sector;
                fat_entry[sector_offset] = next_address;
                writesector(fd, fat_sector, reserved_area_sectors + sector_no);

                // Put the integer of next_address in the FAT table
                unsigned char new_fat_sector[SECTORSIZE];
                readsector(fd, new_fat_sector, reserved_area_sectors + next_sector_no);
                unsigned int *new_fat_entry = (unsigned int *)new_fat_sector;
                new_fat_entry[next_sector_offset] = 0x0fffffff;
                writesector(fd, new_fat_sector, reserved_area_sectors + next_sector_no);

                empty_cluster = new_empty_cluster;
            }
        }
    }
    else
    {
        // Find the start cluster int in FAT Table
        int sector_no = start_cluster / (SECTORSIZE / 4);
        int sector_offset = start_cluster % (SECTORSIZE / 4);

        // Read the value in the FAT Table integer
        unsigned char fat_sector[SECTORSIZE];
        readsector(fd, fat_sector, reserved_area_sectors + sector_no);
        unsigned int *fat_entry = (unsigned int *)fat_sector;
        int next_address = fat_entry[sector_offset];

        // Read the first data cluster
        unsigned char data_cluster[CLUSTERSIZE];
        readCluster(fd, data_cluster, root_start_cluster + start_cluster - 2);
        int remaining_data = n;

        // Put data starting from the offset
        for (int i = offset; i < CLUSTERSIZE && 0 < remaining_data; i++)
        {
            data_cluster[i] = data;
            remaining_data--;
        }

        offset -= CLUSTERSIZE;
        if (offset < 0)
        {
            offset = 0;
        }

        // Write first data cluster
        writeCluster(fd, data_cluster, root_start_cluster + start_cluster - 2);
        printf("Written to cluster %d\n", root_start_cluster + start_cluster - 2);

        // While there are more data to be written, find and write new clusters
        while (remaining_data > 0)
        {
            // Check if there is a next cluster
            if (next_address == 0x0fffffff)
            {
                // Find a new empty cluster
                int new_empty_cluster = -1;
                for (int i = 0; i < root_start_cluster - reserved_area_sectors; i++)
                {
                    unsigned char fat_sector[SECTORSIZE];
                    readsector(fd, fat_sector, reserved_area_sectors + i);
                    unsigned int *fat_entry = (unsigned int *)fat_sector;
                    for (int j = 0; j < SECTORSIZE / 4; j++)
                    {
                        if (fat_entry[j] == 0)
                        {
                            new_empty_cluster = i * SECTORSIZE / 4 + j;
                            break;
                        }
                    }
                }

                // Prepare the int to contain the address of new_empty_cluster. Bits 7-31 tell which sector to read from fat. Bits 0-6 tell which of the 128 integers in that sector contain the number of the next cluster of your file
                int next_sector_no = new_empty_cluster / (SECTORSIZE / 4);
                int next_sector_offset = new_empty_cluster % (SECTORSIZE / 4);
                int next_address = (next_sector_no << 7) | next_sector_offset;

                // Put the number of the new cluster to the previous cluster.
                unsigned char fat_sector[SECTORSIZE];
                readsector(fd, fat_sector, reserved_area_sectors + sector_no);
                unsigned int *fat_entry = (unsigned int *)fat_sector;
                fat_entry[sector_offset] = next_address;
                writesector(fd, fat_sector, reserved_area_sectors + sector_no);

                // Put the integer of next_address in the FAT table
                unsigned char new_fat_sector[SECTORSIZE];
                readsector(fd, new_fat_sector, reserved_area_sectors + next_sector_no);
                unsigned int *new_fat_entry = (unsigned int *)new_fat_sector;
                new_fat_entry[next_sector_offset] = 0x0fffffff;
                writesector(fd, new_fat_sector, reserved_area_sectors + next_sector_no);
                next_address = 0x0fffffff;

                // Put data to the new cluster
                unsigned char new_data_cluster[CLUSTERSIZE];
                for (int i = offset; i < CLUSTERSIZE && 0 < remaining_data; i++)
                {
                    new_data_cluster[i] = data;
                    remaining_data--;
                }
                offset -= CLUSTERSIZE;
                if (offset < 0)
                {
                    offset = 0;
                }
                writeCluster(fd, new_data_cluster, root_start_cluster + new_empty_cluster - 2);
                printf("Written to cluster %d\n", root_start_cluster + new_empty_cluster - 2);

                sector_no = next_sector_no;
                sector_offset = next_sector_offset;
            }
            else
            {
                // Parse the next cluster sector and offset from next_address
                sector_no = next_address >> 7;
                sector_offset = next_address & 0x7f;

                // Read the value in the FAT Table integer
                unsigned char fat_sector[SECTORSIZE];
                readsector(fd, fat_sector, reserved_area_sectors + sector_no);
                unsigned int *fat_entry = (unsigned int *)fat_sector;
                next_address = fat_entry[sector_offset];

                // Calculate the data cluster number
                int next_cluster = sector_no * (SECTORSIZE / 4) + sector_offset;

                // Put data to the next cluster
                unsigned char next_data_cluster[CLUSTERSIZE];
                for (int i = offset; i < CLUSTERSIZE && 0 < remaining_data; i++)
                {
                    next_data_cluster[i] = data;
                    remaining_data--;
                }
                offset -= CLUSTERSIZE;
                if (offset < 0)
                {
                    offset = 0;
                }
                writeCluster(fd, next_data_cluster, root_start_cluster + next_cluster - 2);
                printf("Written to cluster %d\n", root_start_cluster + next_cluster - 2);

                // If no more data to be written, put 0x0fffffff to the last cluster in the FAT table
                if (remaining_data == 0)
                {
                    unsigned char last_fat_sector[SECTORSIZE];
                    readsector(fd, last_fat_sector, reserved_area_sectors + sector_no);
                    unsigned int *last_fat_entry = (unsigned int *)last_fat_sector;
                    last_fat_entry[sector_offset] = 0x0fffffff;
                    writesector(fd, last_fat_sector, reserved_area_sectors + sector_no);
                }
            }
        }
    }
}

void findRootDirectory(int fd)
{
    unsigned char sector[SECTORSIZE];
    // read the boot sector
    readsector(fd, sector, 0);

    struct fat_boot_sector *bp;
    unsigned char num_fats, sectors_per_cluster;
    unsigned int num_sectors, sectors_per_fat;

    bp = (struct fat_boot_sector *)sector;
    sectors_per_cluster = bp->sec_per_clus;
    num_sectors = bp->total_sect;
    num_fats = bp->fats;
    sectors_per_fat = bp->fat32.length;
    root_start_cluster = bp->fat32.root_cluster;

    // Calculate the start of the data section
    reserved_area_sectors = bp->reserved;
    unsigned int fat_size_sectors = bp->fat32.length * num_fats;
    unsigned int data_start_sector = reserved_area_sectors + fat_size_sectors;

    // Convert the data start sector to the cluster number
    root_start_cluster = data_start_sector / sectors_per_cluster;
}

int readsector(int fd, unsigned char *buf, unsigned int snum)
{
    off_t offset;
    int n;
    offset = snum * SECTORSIZE;
    lseek(fd, offset, SEEK_SET);
    n = read(fd, buf, SECTORSIZE);
    if (n == SECTORSIZE)
        return (0);
    else
        return (-1);
}

int readCluster(int fd, unsigned char *buf, unsigned int cnum)
{
    off_t offset;
    int n;
    offset = cnum * CLUSTERSIZE;
    lseek(fd, offset, SEEK_SET);
    n = read(fd, buf, CLUSTERSIZE);
    if (n == CLUSTERSIZE)
        return (0);
    else
        return (-1);
}

int writesector(int fd, unsigned char *buf, unsigned int snum)
{
    off_t offset;
    int n;
    offset = snum * SECTORSIZE;
    lseek(fd, offset, SEEK_SET);
    n = write(fd, buf, SECTORSIZE);
    fsync(fd);
    if (n == SECTORSIZE)
        return (0);
    else
        return (-1);
}

int writeCluster(int fd, unsigned char *buf, unsigned int cnum)
{
    off_t offset;
    int n;
    offset = cnum * CLUSTERSIZE;
    lseek(fd, offset, SEEK_SET);
    n = write(fd, buf, CLUSTERSIZE);
    fsync(fd);
    if (n == CLUSTERSIZE)
        return (0);
    else
        return (-1);
}

char *ltrim(char *s)
{
    while (isspace(*s))
        s++;
    return s;
}

char *rtrim(char *s)
{
    char *back = s + strlen(s);
    while (isspace(*--back))
        ;
    *(back + 1) = '\0';
    return s;
}

char *trim(char *s)
{
    return rtrim(ltrim(s));
}