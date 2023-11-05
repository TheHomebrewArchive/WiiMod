#ifndef _FILESYSTEM_H_
#define _FILESYSTEM_H_

#ifdef __cplusplus
extern "C" {
#endif

bool FileExists(const char* path);

#define FileReadBinary(path, outbuf, outlen) FileRead(path, "rb", outbuf, outlen);
#define FileReadNormal(path, outbuf, outlen) FileRead(path, "r", outbuf, outlen);

int FileRead(const char* path, const char* mode, u8 **outbuf, u32 *outlen);
bool FolderCreateTree(const char *fullpath);

#ifdef __cplusplus
}
#endif

#endif
