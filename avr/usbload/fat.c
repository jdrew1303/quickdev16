/*
 * ####################################################################################### FAT for AVR (MMC/SD)   Copyright (C) 2004
 * Ulrich Radig  Bei Fragen und Verbesserungen wendet euch per EMail an  mail@ulrichradig.de  oder im Forum meiner Web Page :
 * www.ulrichradig.de   Dieses Programm ist freie Software. Sie k�nnen es unter den Bedingungen der  GNU General Public License, wie
 * von der Free Software Foundation ver�ffentlicht,  weitergeben und/oder modifizieren, entweder gem�� Version 2 der Lizenz oder  (nach
 * Ihrer Option) jeder sp�teren Version.   Die Ver�ffentlichung dieses Programms erfolgt in der Hoffnung,  da� es Ihnen von Nutzen sein
 * wird, aber OHNE IRGENDEINE GARANTIE,  sogar ohne die implizite Garantie der MARKTREIFE oder der VERWENDBARKEIT  F�R EINEN BESTIMMTEN
 * ZWECK. Details finden Sie in der GNU General Public License.   Sie sollten eine Kopie der GNU General Public License zusammen mit
 * diesem  Programm erhalten haben.  Falls nicht, schreiben Sie an die Free Software Foundation,  Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.  #######################################################################################
 */

#include "fat.h"
uint8_t cluster_size;
uint16_t fat_offset;
uint16_t cluster_offset;
uint16_t volume_boot_record_addr;

    // ############################################################################
    // Auslesen Cluster Size der MMC/SD Karte und Speichern der gr��e ins EEprom
    // Auslesen Cluster Offset der MMC/SD Karte und Speichern der gr��e ins EEprom
void fat_init(uint8_t * Buffer)
// ############################################################################
{
    struct BootSec *bootp;      // Zeiger auf Bootsektor Struktur

    // uint8_t Buffer[BlockSize];

    // volume_boot_record_addr = fat_addr (Buffer); 
    mmc_read_sector(MASTER_BOOT_RECORD, Buffer);        // Read Master Boot Record 
    if (Buffer[510] == 0x55 && Buffer[511] == 0xAA) {
        FAT_DEBUG("MBR Signatur found!\r\n");
    }

    else {
        FAT_DEBUG("MBR Signatur not found!\r\n");
        while (1);
    }
    volume_boot_record_addr = Buffer[VBR_ADDR] + (Buffer[VBR_ADDR + 1] << 8);
    mmc_read_sector(volume_boot_record_addr, Buffer);
    if (Buffer[510] == 0x55 && Buffer[511] == 0xAA) {
        FAT_DEBUG("VBR Signatur found!\r\n");
    }

    else {
        FAT_DEBUG("VBR Signatur not found!\r\n");
        volume_boot_record_addr = MASTER_BOOT_RECORD;   // <- added by Hennie
        mmc_read_sector(MASTER_BOOT_RECORD, Buffer);    // Read Master Boot Record 
    }
    bootp = (struct BootSec *) Buffer;
    cluster_size = bootp->BPB_SecPerClus;
    fat_offset = bootp->BPB_RsvdSecCnt;
    cluster_offset = ((bootp->BPB_BytesPerSec * 32) / BlockSize);
    cluster_offset += fat_root_dir_addr(Buffer);
}
    // ############################################################################
    // Auslesen der Adresse des First Root Directory von Volume Boot Record
uint16_t fat_root_dir_addr(uint8_t *Buffer)
// ############################################################################
{
    struct BootSec *bootp;      // Zeiger auf Bootsektor Struktur
    uint16_t FirstRootDirSecNum;

    // auslesen des Volume Boot Record von der MMC/SD Karte 
    mmc_read_sector(volume_boot_record_addr, Buffer);
    bootp = (struct BootSec *) Buffer;

    // berechnet den ersten Sector des Root Directory
    FirstRootDirSecNum = (bootp->BPB_RsvdSecCnt +
                          (bootp->BPB_NumFATs * bootp->BPB_FATSz16));
    FirstRootDirSecNum += volume_boot_record_addr;
    return (FirstRootDirSecNum);
}

    // ############################################################################
    // Ausgabe des angegebenen Directory Eintrag in Entry_Count
    // ist kein Eintrag vorhanden, ist der Eintrag im 
    // R�ckgabe Cluster 0xFFFF. Es wird immer nur ein Eintrag ausgegeben
    // um Speicherplatz zu sparen um es auch f�r kleine Atmels zu benutzen
uint16_t fat_read_dir_ent(uint16_t dir_cluster, // Angabe Dir Cluster
                              uint8_t Entry_Count,        // Angabe welcher Direintrag
                              uint32_t *Size,      // R�ckgabe der File Gr��e
                              uint8_t *Dir_Attrib,        // R�ckgabe des Dir Attributs
                              uint8_t *Buffer)    // Working Buffer
// ############################################################################
{
    uint8_t *pointer;
    uint16_t TMP_Entry_Count = 0;
    uint32_t Block = 0;
    struct DirEntry *dir;       // Zeiger auf einen Verzeichniseintrag
    uint16_t blk;
    uint16_t a;
    uint8_t b;
    pointer = Buffer;
    if (dir_cluster == 0) {
        Block = fat_root_dir_addr(Buffer);
    }

    else {

        // Berechnung des Blocks aus BlockCount und Cluster aus FATTabelle
        // Berechnung welcher Cluster zu laden ist
        // Auslesen der FAT - Tabelle
        fat_load(dir_cluster, &Block, Buffer);
        Block = ((Block - 2) * cluster_size) + cluster_offset;
    }

    // auslesen des gesamten Root Directory
    for (blk = Block;; blk++) {
        mmc_read_sector(blk, Buffer);   // Lesen eines Blocks des Root Directory
        for (a = 0; a < BlockSize; a = a + 32) {
            dir = (struct DirEntry *) &Buffer[a];       // Zeiger auf aktuellen Verzeichniseintrag holen
            if (dir->DIR_Name[0] == 0)  // Kein weiterer Eintrag wenn erstes Zeichen des Namens 0 ist
            {
                return (0xFFFF);
            }
            // Pr�fen ob es ein 8.3 Eintrag ist
            // Das ist der Fall wenn es sich nicht um einen Eintrag f�r lange Dateinamen
            // oder um einen als gel�scht markierten Eintrag handelt.
            if ((dir->DIR_Attr != ATTR_LONG_NAME) &&
                (dir->DIR_Name[0] != DIR_ENTRY_IS_FREE)) {

                // Ist es der gew�nschte Verzeichniseintrag
                if (TMP_Entry_Count == Entry_Count) {

                    // Speichern des Verzeichnis Eintrages in den R�ckgabe Buffer
                    for ( b = 0; b < 11; b++) {
                        if (dir->DIR_Name[b] != SPACE) {
                            if (b == 8) {
                                *pointer++ = '.';
                            }
                            *pointer++ = dir->DIR_Name[b];
                        }
                    }
                    *pointer++ = '\0';
                    *Dir_Attrib = dir->DIR_Attr;

                    // Speichern der Filegr��e
                    *Size = dir->DIR_FileSize;

                    // Speichern des Clusters des Verzeichniseintrages
                    dir_cluster = dir->DIR_FstClusLO;

                    // Eintrag gefunden R�cksprung mit Cluster File Start
                    return (dir_cluster);
                }
                TMP_Entry_Count++;
            }
        }
    }
    return (0xFFFF);            // Kein Eintrag mehr gefunden R�cksprung mit 0xFFFF
}

    // ############################################################################
    // Auslesen der Cluster f�r ein File aus der FAT
    // in den Buffer(512Byte). Bei einer 128MB MMC/SD 
    // Karte ist die Cluster gr��e normalerweise 16KB gro�
    // das bedeutet das File kann max. 4MByte gro� sein.
    // Bei gr��eren Files mu� der Buffer gr��er definiert
    // werden! (Ready)
    // Cluster = Start Clusterangabe aus dem Directory 
void fat_load(uint16_t Cluster,     // Angabe Startcluster
              uint32_t *Block, uint8_t *TMP_Buffer)  // Workingbuffer
// ############################################################################
{

    // Zum �berpr�fen ob der FAT Block schon geladen wurde
    uint16_t FAT_Block_Store = 0;

    // Byte Adresse innerhalb des Fat Blocks
    uint16_t FAT_Byte_Addresse;

    // FAT Block Adresse
    uint16_t FAT_Block_Addresse;
    uint16_t a;

    // Berechnung f�r den ersten FAT Block (FAT Start Addresse)
    for (a = 0;; a++) {
        if (a == *Block) {
            *Block = (0x0000FFFF & Cluster);
            return;
        }
        if (Cluster == 0xFFFF) {
            break;              // Ist das Ende des Files erreicht Schleife beenden
        }
        // Berechnung des Bytes innerhalb des FAT Block�s
        FAT_Byte_Addresse = (Cluster * 2) % BlockSize;

        // Berechnung des Blocks der gelesen werden mu�
        FAT_Block_Addresse =
            ((Cluster * 2) / BlockSize) + volume_boot_record_addr + fat_offset;

        // Lesen des FAT Blocks
        // �berpr�fung ob dieser Block schon gelesen wurde
        if (FAT_Block_Addresse != FAT_Block_Store) {
            FAT_Block_Store = FAT_Block_Addresse;

            // Lesen des FAT Blocks
            mmc_read_sector(FAT_Block_Addresse, TMP_Buffer);
        }
        // Lesen der n�chsten Clusternummer
        Cluster =
            (TMP_Buffer[FAT_Byte_Addresse + 1] << 8) +
            TMP_Buffer[FAT_Byte_Addresse];
    }
    return;
}

    // ############################################################################
    // Lesen eines 512Bytes Blocks von einem File
void fat_read_file(uint16_t Cluster,        // Angabe des Startclusters vom File
                   uint8_t *Buffer,       // Workingbuffer
                   uint32_t BlockCount)    // Angabe welcher Bock vom File geladen 
                                                                                      // werden soll a 512 Bytes
// ############################################################################
{

    // Berechnung des Blocks aus BlockCount und Cluster aus FATTabelle
    // Berechnung welcher Cluster zu laden ist
    uint32_t Block = (BlockCount / cluster_size);

    // Auslesen der FAT - Tabelle
    fat_load(Cluster, &Block, Buffer);
    Block = ((Block - 2) * cluster_size) + cluster_offset;

    // Berechnung des Blocks innerhalb des Cluster
    Block += (BlockCount % cluster_size);

    // Read Data Block from Device
    mmc_read_sector(Block, Buffer);
    return;
}

    // ############################################################################
    // Lesen eines 512Bytes Blocks von einem File
void fat_write_file(uint16_t cluster,       // Angabe des Startclusters vom File
                    uint8_t *buffer,      // Workingbuffer
                    uint32_t blockCount)   // Angabe welcher Bock vom File gespeichert 
                                                                          // werden soll a 512 Bytes
// ############################################################################
{

    // Berechnung des Blocks aus BlockCount und Cluster aus FATTabelle
    // Berechnung welcher Cluster zu speichern ist
    uint8_t tmp_buffer[513];
    uint32_t block = (blockCount / cluster_size);

    // Auslesen der FAT - Tabelle
    fat_load(cluster, &block, tmp_buffer);
    block = ((block - 2) * cluster_size) + cluster_offset;

    // Berechnung des Blocks innerhalb des Cluster
    block += (blockCount % cluster_size);

    // Write Data Block to Device
    mmc_write_sector(block, buffer);
    return;
}

    // ####################################################################################
    // Sucht ein File im Directory
uint8_t fat_search_file(uint8_t *File_Name, // Name des zu suchenden Files
                              uint16_t *Cluster,    // Angabe Dir Cluster welches
                              // durchsucht werden soll
                              // und R�ckgabe des clusters
                              // vom File welches gefunden
                              // wurde
                              uint32_t *Size,      // R�ckgabe der File Gr��e
                              uint8_t *Dir_Attrib,        // R�ckgabe des Dir Attributs
                              uint8_t *Buffer)    // Working Buffer
// ####################################################################################
{
    uint16_t Dir_Cluster_Store = *Cluster;
    uint8_t a ;
    for (a = 0; a < 100; a++) {
        *Cluster =
            fat_read_dir_ent(Dir_Cluster_Store, a, Size, Dir_Attrib, Buffer);
        if (*Cluster == 0xffff) {
            return (0);         // File not Found
        }
        if (strcasecmp((char *) File_Name, (char *) Buffer) == 0) {
            return (1);         // File Found
        }
    }
    return (2);                 // Error
}
