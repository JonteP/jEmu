#include "cartridge.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>	/* malloc; exit */
#include <string.h>
#include "parser.h"
#include "tree.h"
#include "sha.h"
#include "globals.h"

static inline void extract_xml_data(xmlNode * s_node), xml_hash_compare(xmlNode * c_node), parse_xml_file(xmlNode * a_node),
				   load_by_header();

/* iNES */
const uint_fast8_t id[4] = { 0x4e, 0x45, 0x53, 0x1a };
uint_fast8_t header[0x10];

/* XML */
xmlDoc *nesXml;
xmlNode *root;
xmlChar *sphash, *schash;
uint_fast8_t hashMatch = 0;

/* common cartridge related */
gameInfos gameInfo;
gameFeatures cart;
int psize, csize;
unsigned char phash[SHA_DIGEST_LENGTH], chash[SHA_DIGEST_LENGTH];
uint_fast8_t *prg, *chrRom, *chrRam, *bwram, *wram, wramEnable = 0, *wramSource;
FILE *romFile, *bwramFile;
char *bwramName;
uint_fast8_t mirroring[4][4] = { { 0, 0, 1, 1 }, 	/* horizontal mirroring 	*/
							{ 0, 1, 0, 1 }, 	/* vertical mirroring 		*/
							{ 0, 0, 0, 0 },		/* one screen, low page 	*/
							{ 1, 1, 1, 1 } };	/* one screen, high page 	*/

/* horizontal:
 * nameSlot[0] = ciram;
 * nameSlot[1] = ciram;
 * nameSlot[2] = ciram + 0x400;
 * nameSlot[3] = ciram + 0x400;
 */

void load_rom(char *rom)
{
	romFile = fopen(rom, "r");
	if (romFile == NULL) {
		printf("Error: No such file\n");
		exit(EXIT_FAILURE);
	}

	const char *ext = strrchr(rom, '.') + 1;
	if (!strcmp(ext,"nes"))
	{
		/* if file has .nes extension */
		fread(header, sizeof(header), 1, romFile);
		for (int i = 0; i < sizeof(id); i++)
		{
			if (header[i] != id[i])
			{
				fprintf(stderr,"Error: Invalid iNES header!\n");
				exit(EXIT_FAILURE);
			}
		}
		/* read data from ROM */
		psize = header[4] * PRG_BANK<<2;
		csize = header[5] * CHR_BANK<<3;
	}

	prg = malloc(psize);
	fread(prg, psize, 1, romFile);
	SHA1(prg,psize,phash);

	sphash = malloc(SHA_DIGEST_LENGTH*2+1);
	for (int i = 0; i<sizeof(phash); i++)
	{
		sprintf((char *)sphash+i*2, "%02x", phash[i]);
	}
	nesXml = xmlReadFile("/home/jonas/git/nesemu0.2/Debug/nes.xml", NULL, 0);
	root = xmlDocGetRootElement(nesXml);
	parse_xml_file(root);
	xmlFreeDoc(nesXml);
	xmlCleanupParser();

	if (!hashMatch) {
		load_by_header();
	}

	if (cart.chrSize)
	{
		chrRom = malloc(cart.chrSize);
		fread(chrRom, cart.chrSize, 1, romFile);
	}
	if (cart.cramSize)
	{
		chrRam = malloc(cart.cramSize);
	}
	fclose(romFile);

	if (psize < cart.prgSize)
		cart.prgSize = psize;
	cart.pSlots = ((cart.prgSize) / 0x1000);
	cart.cSlots = ((cart.chrSize) / 0x400);

	if (cart.bwramSize) {
		bwramName = strdup(rom);
		sprintf(bwramName+strlen(bwramName)-3,"sav");
		bwramFile = fopen(bwramName, "r");
		bwram = malloc(cart.bwramSize);
		if (bwramFile) {
			fread(bwram, cart.bwramSize, 1, bwramFile);
			fclose(bwramFile);
		}
		wramSource = bwram;
		wramEnable = 1;
	} else if (cart.wramSize) {
		wram = malloc(cart.wramSize);
		wramSource = wram;
		wramEnable = 1;
	} else
		wramEnable = 0;
	printf("PCB: %s (%s)\n",cart.pcb,cart.slot);
	printf("PRG size: %li bytes\n",cart.prgSize);
	printf("CHR size: %li bytes\n",cart.chrSize);
	printf("WRAM size: %li bytes\n",cart.wramSize);
	printf("BWRAM size: %li bytes\n",cart.bwramSize);
	printf("CHRRAM size: %li bytes\n",cart.cramSize);
}

void close_rom()
{
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
	} else if (cart.wramSize) {
		free(wram);
	}
}

void set_wram()
{
	if (header[6] & 0x02)
	{
		cart.wramSize = 0;
		cart.bwramSize = 0x2000;
	}
	else
	{
		cart.wramSize = 0x2000;
		cart.bwramSize = 0;
	}
}

void load_by_header()
{
	uint_fast8_t mapper = ((header[7] & 0xf0) | ((header[6] & 0xf0) >> 4));
	printf("Mapper: %i\n",mapper);
	cart.prgSize = psize;
	if (csize)
	{
		cart.chrSize = csize;
		cart.cramSize = 0;
	}
	else
	{
		cart.chrSize = 0;
		cart.cramSize = 0x2000;
	}
	if (header[6] & 0x08)
		cart.mirroring = 4;
	else
		cart.mirroring = (header[6] & 0x01);
	switch (mapper)
	{
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
		if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (xmlChar *)"feature")) {
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
				strcpy(cart.mmc1_type,(char *)val);
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

		} else if (cur_node->type == XML_ELEMENT_NODE && !xmlStrcmp(cur_node->name, (xmlChar *)"dataarea")) {
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

void xml_hash_compare(xmlNode * c_node)
{
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
void parse_xml_file(xmlNode * a_node)
{
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
