#include "nescartridge.h"
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"
#include "tree.h"
#include "sha.h"
#include "nesemu.h"

//UNIF
#define UNIF_TYPE_SIZE      4
#define UNIF_LENGTH_SIZE    4
static const uint8_t unifId[4] = {0x55, 0x4e, 0x49, 0x46};
static uint8_t isUnif;

//iNES
static const uint8_t inesId[4] = {0x4e, 0x45, 0x53, 0x1a};
static uint8_t isInes;

static uint8_t header[0x10];

//XML
xmlDoc *nesXml;
xmlNode *root;
xmlChar *sphash = NULL, *schash = NULL;
uint8_t hashMatch;

//common cartridge related
gameFeatures cart;
static unsigned char phash[SHA_DIGEST_LENGTH];

//on cartridge memory sources
uint8_t *prg = NULL;
uint8_t *bwram = NULL;
uint8_t *wram = NULL;
uint8_t wramEnable;
uint8_t *wramSource = NULL; //TODO: should be made obsolete
uint8_t *chrRom = NULL;
uint8_t *chrRam = NULL;

static FILE *romFile;
static FILE *bwramFile; //backed-up RAM assumed to have same name as ROM with .sav extension
char *bwramName;

//Nametable mirroring - basically sets the routing of CIRAM A10
uint8_t mirroring[4][4] =
	{ { 0, 0, 1, 1 }, 	// horizontal mirroring  CIRAM A10 <-> PPU A10
	  { 0, 1, 0, 1 }, 	// vertical mirroring    CIRAM A10 <-> PPU A11
	  { 0, 0, 0, 0 },	// one screen, low page  CIRAM A10 <-> Ground
	  { 1, 1, 1, 1 } };	// one screen, high page CIRAM A10 <-> Vcc

static inline void extract_xml_data(xmlNode * s_node);
static inline void xml_hash_compare(xmlNode * c_node);
static inline void parse_xml_file(xmlNode * a_node);
static inline void load_by_header();

void nes_load_rom(char *rom) {
    cart.bwramSize = 0;
    cart.battery = 0;
    cart.wramSize = 0;
    cart.chrSize = 0;
    cart.cramSize = 0;
    cart.prgSize = 0;
    cart.chrSize = 0;
    isInes = 0;
    isUnif = 0;
    hashMatch = 0;
    wramEnable = 0;
    if ((romFile = fopen(rom, "r")) == NULL) {
        printf("Error: No such file\n");
        exit(EXIT_FAILURE);
    }

	//look for iNES header
    fread(header, sizeof(header), 1, romFile);
    for (int i = 0; i < sizeof(inesId); i++) {
        if (header[i] != inesId[i]) {
            isInes = 1;
            break;
        }
    }
    if(!isInes) { //found iNES header
        cart.prgSize = header[4] * PRG_BANK << 2;
        cart.chrSize = header[5] * CHR_BANK << 3;
        free(prg);
        prg = malloc(cart.prgSize * sizeof(uint8_t));
        fread(prg, cart.prgSize, 1, romFile);
        if (cart.chrSize) {
            free(chrRom);
            chrRom = malloc(cart.chrSize * sizeof(uint8_t));
            fread(chrRom, cart.chrSize, 1, romFile);
        }
    }

    else { //look for Unif header
        for (int i = 0; i < sizeof(unifId); i++) {
            if (header[i] != unifId[i]) {
                isUnif = 1;
                break;
            }
        }
        if(!isUnif) { //found Unif header
            fseek(romFile, 0, SEEK_END);
            uint32_t fSize = ftell(romFile);
            char blockType[UNIF_TYPE_SIZE + 1];
            cart.prgSize = 0;
            cart.chrSize = 0;
            uint32_t blockLength;
            fseek(romFile,0x20,SEEK_SET);
            while (ftell(romFile) < fSize) {
                fread(blockType, UNIF_TYPE_SIZE, 1, romFile);
                blockType[4] = '\0';
                fread(&blockLength, UNIF_LENGTH_SIZE, 1, romFile);
                if(!strcmp(blockType, "MAPR")) {
                    char *map = malloc(blockLength);
                    fread(map, blockLength, 1, romFile);
                    strcpy(cart.slot,map);
                    free(map);
                }
                else if(!strcmp(blockType, "NAME")) {
                    char *name = malloc(blockLength);
                    fread(name, blockLength, 1, romFile);
                    printf("Name: %s\n",name);
                    free(name);
                }
                else if(!strcmp(blockType, "TVCI")) {
                    assert(blockLength == 1);
                    uint8_t tv;
                    fread(&tv, blockLength, 1, romFile);
                }
                else if(!strncmp(blockType, "PRG", 3)) {
                    prg = realloc(prg, sizeof(uint8_t) * (cart.prgSize + blockLength));
                    fread(prg + cart.prgSize, blockLength, 1, romFile);
                    cart.prgSize += blockLength;
               }
                else if(!strncmp(blockType, "CHR", 3)) {
                    chrRom = realloc(chrRom, sizeof(uint8_t) * (cart.chrSize + blockLength));
                    fread(chrRom + cart.chrSize, blockLength, 1, romFile);
                    cart.chrSize += blockLength;
                }
                else if(!strcmp(blockType, "BATR")) {
                    assert(blockLength == 1);
                    fread(&cart.battery, blockLength, 1, romFile);
                }
                else if(!strcmp(blockType, "MIRR")) {
                    assert(blockLength == 1);
                    fread(&cart.mirroring, blockLength, 1, romFile);
                }
                else
                    break;
            }
        }
        if(!strcmp(cart.slot, "KONAMI-QTAI")) {
            cart.bwramSize = 8192;
            cart.wramSize = 8192;
            cart.cramSize = 8192;
        }
    }

//look for match in softlist
    SHA1(prg,cart.prgSize,phash);
    free(sphash);
    sphash = malloc(SHA_DIGEST_LENGTH * 2 + 1);
    for (int i = 0; i<sizeof(phash); i++){
        sprintf((char *)sphash+i*2, "%02x", phash[i]);
    }
    nesXml = xmlReadFile("softlist/nes.xml", NULL, 0);
    root = xmlDocGetRootElement(nesXml);
    parse_xml_file(root);
    xmlFreeDoc(nesXml);
    xmlCleanupParser();

    if (!hashMatch && !isInes)
        load_by_header();

    if (cart.cramSize) {
        free(chrRam);
        chrRam = malloc(cart.cramSize * sizeof(uint8_t));
    }
    fclose(romFile);

    cart.pSlots = ((cart.prgSize) / 0x1000);
    cart.cSlots = ((cart.chrSize) / 0x400);

    if (cart.bwramSize) {
        bwramName = strdup(rom);
        sprintf(bwramName+strlen(bwramName)-3,"sav");
        bwramFile = fopen(bwramName, "r");
        free(bwram);
        bwram = malloc(cart.bwramSize * sizeof(uint8_t));
        if (bwramFile) {
            fread(bwram, cart.bwramSize, 1, bwramFile);
            fclose(bwramFile);
        }
        wramSource = bwram;
        wramEnable = 1;
    }
    if (cart.wramSize) {
        free(wram);
        wram = malloc(cart.wramSize);
        wramSource = wram;
        wramEnable = 1;
    }
    if(!(cart.bwramSize) && !(cart.wramSize))
        wramEnable = 0;
    printf("PCB: %s (%s)\n",cart.pcb,cart.slot);
    printf("PRG size: %li bytes\n",cart.prgSize);
    printf("CHR size: %li bytes\n",cart.chrSize);
    printf("WRAM size: %li bytes\n",cart.wramSize);
    printf("BWRAM size: %li bytes\n",cart.bwramSize);
    printf("CHRRAM size: %li bytes\n",cart.cramSize);
}

void nes_close_rom() {
    free(prg);
    if (cart.chrSize)
        free(chrRom);
    if (cart.cramSize)
        free(chrRam);
    if (cart.bwramSize) {
        bwramFile = fopen(bwramName, "w");
        fwrite(bwram,cart.bwramSize,1,bwramFile);
        free(bwram);
        fclose(bwramFile);
    }
    else if (cart.wramSize)
        free(wram);
}

void set_wram() {
    if (header[6] & 0x02) {
        cart.wramSize = 0;
        cart.bwramSize = 0x2000;
    }
    else {
        cart.wramSize = 0x2000;
        cart.bwramSize = 0;
    }
}

void load_by_header() {
    uint8_t mapper = ((header[7] & 0xf0) | ((header[6] & 0xf0) >> 4));
    printf("Mapper: %i\n",mapper);
    if (cart.chrSize) {
        cart.cramSize = 0;
    }
    else {
        cart.chrSize = 0;
        cart.cramSize = 0x2000;
    }
    if (header[6] & 0x08)
        cart.mirroring = 4;
    else
        cart.mirroring = (header[6] & 0x01);
    switch (mapper) {
    case 0:
        sprintf(cart.slot,"%s","nrom");
        break;
    case 1:
        sprintf(cart.slot,"%s","sxrom");
        set_wram();
        break;
    case 3:
        sprintf(cart.slot,"%s","unrom");
        break;
    case 4:
        sprintf(cart.slot,"%s","txrom");
        set_wram();
        break;
    case 21:
        sprintf(cart.slot,"%s","vrc4");
        set_wram();
        cart.vrc24Prg1 = 7;
        cart.vrc24Prg0 = 6;
        break;
    case 24:
        sprintf(cart.slot,"%s","vrc6");
        break;
    case 180:
        sprintf(cart.slot,"%s","unrom_cc");
        break;
    default:
        fprintf(stderr,"Error: Unknown mapper (%i)!",mapper);
        exit(EXIT_FAILURE);
    }
}

void extract_xml_data(xmlNode * s_node) {
    xmlNode *cur_node = s_node->children;
    xmlChar *nam, *val;
    while (cur_node) {
        if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (xmlChar *)"feature")){
            nam = xmlGetProp(cur_node, (xmlChar *)"name");
            val = xmlGetProp(cur_node, (xmlChar *)"value");
            if (!xmlStrcmp(nam,(xmlChar *)"slot"))
                strcpy(cart.slot,(char *)val);
            else if (!xmlStrcmp(nam,(xmlChar *)"pcb"))
                strcpy(cart.pcb,(char *)val);
            else if (!xmlStrcmp(nam,(xmlChar *)"vrc2-pin3"))
                cart.vrc24Prg1 = strtol((char *)val+5,NULL,10);
            else if (!xmlStrcmp(nam,(xmlChar *)"vrc2-pin4"))
                cart.vrc24Prg0 = strtol((char *)val+5,NULL,10);
            else if (!xmlStrcmp(nam,(xmlChar *)"vrc4-pin3"))
                cart.vrc24Prg1 = strtol((char *)val+5,NULL,10);
            else if (!xmlStrcmp(nam,(xmlChar *)"vrc4-pin4"))
                cart.vrc24Prg0 = strtol((char *)val+5,NULL,10);
            else if (!xmlStrcmp(nam,(xmlChar *)"vrc2-pin21")) {
                if (!xmlStrcmp(val,(xmlChar *)"NC"))
                    cart.vrc24Chr = 0;
                else
                    cart.vrc24Chr = 1;
            }
            else if (!xmlStrcmp(nam,(xmlChar *)"vrc6-pin9"))
                cart.vrc6Prg1 = strtol((char *)val+5,NULL,10);
            else if (!xmlStrcmp(nam,(xmlChar *)"vrc6-pin10"))
                cart.vrc6Prg0 = strtol((char *)val+5,NULL,10);
            else if (!xmlStrcmp(nam,(xmlChar *)"mmc1_type"))
                strcpy(cart.subtype,(char *)val);
            else if (!xmlStrcmp(nam,(xmlChar *)"mmc3_type"))
                strcpy(cart.subtype,(char *)val);
            else if (!xmlStrcmp(nam,(xmlChar *)"mirroring")) {
                if (!xmlStrcmp(val,(xmlChar *)"horizontal"))
                    cart.mirroring = 0;
                else if (!xmlStrcmp(val,(xmlChar *)"vertical"))
                    cart.mirroring = 1;
                else if (!xmlStrcmp(val,(xmlChar *)"high"))
                    cart.mirroring = 3;
                else if (!xmlStrcmp(val,(xmlChar *)"4screen"))
                    cart.mirroring = 4;
                else if (!xmlStrcmp(val,(xmlChar *)"pcb_controlled"))
                    cart.mirroring = 5;
            }
            xmlFree(nam);
            xmlFree(val);
        }
        else if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (xmlChar *)"dataarea")) {
            nam = xmlGetProp(cur_node, (xmlChar *)"name");
            val = xmlGetProp(cur_node, (xmlChar *)"size");
            if (!xmlStrcmp(nam,(xmlChar *)"prg"))
                cart.prgSize = strtol((char *)val,NULL,10);
            else if (!xmlStrcmp(nam,(xmlChar *)"chr"))
                cart.chrSize = strtol((char *)val,NULL,10);
            else if (!xmlStrcmp(nam,(xmlChar *)"wram"))
                cart.wramSize = strtol((char *)val,NULL,10);
            else if (!xmlStrcmp(nam,(xmlChar *)"bwram"))
                cart.bwramSize = strtol((char *)val,NULL,10);
            else if (!xmlStrcmp(nam,(xmlChar *)"vram"))
                cart.cramSize = strtol((char *)val,NULL,10);
            else if (!xmlStrcmp(nam,(xmlChar *)"vram2"))
                cart.cramSize += strtol((char *)val,NULL,10);
            xmlFree(nam);
            xmlFree(val);
        }
        cur_node = cur_node->next;
    }
}

void xml_hash_compare(xmlNode * c_node) {
    xmlNode *cur_node = NULL;
    xmlChar *hashkey;
    for (cur_node = c_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (const xmlChar *) "rom")) {
            hashkey = xmlGetProp(cur_node, (xmlChar *)"sha1");
            if (!xmlStrcmp(hashkey,sphash)) {
                hashMatch = 1;
                extract_xml_data(cur_node->parent->parent);
            }
            xmlFree(hashkey);
        }
    }
}
void parse_xml_file(xmlNode * a_node) {
    xmlNode *cur_node = NULL;
    xmlChar *key;
    for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
        if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (const xmlChar *) "dataarea")) {
            key = xmlGetProp(cur_node, (xmlChar *)"name");
            if (!xmlStrcmp(key, (xmlChar *)"prg")) {
                xml_hash_compare(cur_node->children);
            }
            xmlFree(key);
        }
        parse_xml_file(cur_node->children);
    }
}
