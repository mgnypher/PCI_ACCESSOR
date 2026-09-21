#include <stdio.h>
#include <sys/mman.h>
#include <pci/pci.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>




struct pci_dev* find_pci_device(const char* fulldev_name, struct pci_access* pacc)
{
    int uDOMAIN;
    int uBUS;
    int uDEV;
    int uDEVFUNC;
    struct pci_dev *dev;

    sscanf(fulldev_name, "%x:%x:%x.%x", &uDOMAIN, &uBUS, &uDEV, &uDEVFUNC);

    for (dev = pacc->devices; dev; dev = dev->next) 
    {
        pci_fill_info(dev, PCI_FILL_IDENT | PCI_FILL_BASES | PCI_FILL_SIZES);

        if (dev->domain == uDOMAIN && dev->dev == uDEV && dev->bus == uBUS && dev->func == uDEVFUNC)
        {
            return dev;
        }
    }

    return NULL;
}



void res0_mmaper(const char* pci_name, struct pci_dev* dev)
{
    const char* resource = "resource0";
    char path[256];                                                                   // Буфер, хранящий путь к файлу
    size_t BAR_MEMORY_SIZE = dev->size[0];

    snprintf(path, sizeof(path), "/sys/bus/pci/devices/%s/%s", pci_name, resource);   // Записываем путь в буфер path


    int file_to_work_with = open(path, O_RDWR | O_SYNC);

    if (file_to_work_with == -1)
    {
        perror("open");
        return;
    }

    printf("resource0 OPENED!\n");


    // Сам маппинг

    void *MMAPED_ENTITY = mmap(NULL, BAR_MEMORY_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, file_to_work_with, 0);

    if (MMAPED_ENTITY == MAP_FAILED)
    {
        perror("mmap");
        close(file_to_work_with);
        return;
    }

    printf("BAR MAPPED AT %p (SIZE = %zu)\n\n", MMAPED_ENTITY, BAR_MEMORY_SIZE);


    volatile uint32_t *BAR0 = (volatile uint32_t* )MMAPED_ENTITY;

    printf("FIRST 8 REGISTERS:\n");

    for (int i = 0; i < 8; i++)
    {
        printf("OFFSET 0x%02x: 0x%08x\n", i*4, BAR0[i]);
    }

    munmap(MMAPED_ENTITY, BAR_MEMORY_SIZE);
    close(file_to_work_with);
}





int main(int argc, char* argv[])
{
    struct pci_access *pacc;
    struct pci_dev *device_found = NULL;


    printf("\n");

    // Если не указан аргумент (не указано устройство) - то выводим ошибку, чтобы избежать segmentation fault

    if (argc < 2)
    {
        printf("Cannot execute program. Specify the needed device.\n");
        return 1;
    }


    pacc = pci_alloc();
    pci_init(pacc);
    pci_scan_bus(pacc);


    // Поиск устройства

    device_found = find_pci_device(argv[1], pacc);

    if (device_found == NULL)
    {
        printf("Device not found. Check the connection.\n\n");
        pci_cleanup(pacc);
        return 0;
    }

    printf("Device Found: %04x:%02x:%02x.%d\n", device_found->domain, device_found->bus, device_found->dev, device_found->func);
    printf("BAR0 base: 0x%lx, size: %lu\n", (unsigned long)device_found->base_addr[0], (unsigned long)device_found->size[0]);

    printf("\n");

    // Маппинг

    res0_mmaper(argv[1], device_found);


    pci_cleanup(pacc);
    return 0;
}