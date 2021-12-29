#ifndef CPMDIR_H
#define CPMDIR_H

struct PhysDirectoryEntry {
	unsigned char status;
	unsigned char name[8];
	unsigned char ext[3];
	unsigned char extnol;
	unsigned char lrc;
	unsigned char extnoh;
	unsigned char blkcnt;
	unsigned char pointers[16];
};

#define ISFILECHAR(notFirst,c) (((notFirst) || (c)!=' ') && (c)>=' ' && !((c)&~0x7f) && (c)!='<' && (c)!='>' && (c)!='.' && (c)!=',' && (c)!=';' && (c)!=':' && (c)!='=' && (c)!='?' && (c)!='*' && (c)!= '[' && (c)!=']')
#define EXTENT(low,high) (((low)&0x1f)|(((high)&0x3f)<<5))
#define EXTENTL(extent) ((extent)&0x1f)
#define EXTENTH(extent) (((extent>>5))&0x3f)

#endif
