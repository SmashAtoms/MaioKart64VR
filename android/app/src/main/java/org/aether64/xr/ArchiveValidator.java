package org.aether64.xr;

import java.io.*;
import java.util.*;
import java.util.zip.*;

/** Validates the archive container without extracting paths onto the filesystem. */
public final class ArchiveValidator {
    public static final long MAX_ARCHIVE_BYTES=1024L*1024*1024;
    private static final long MAX_EXPANDED_BYTES=2L*1024*1024*1024;
    private ArchiveValidator() {}
    public static void validate(File archive)throws IOException {
        if(archive.length()<22||archive.length()>MAX_ARCHIVE_BYTES)throw new IOException("Invalid archive size");
        try(RandomAccessFile raw=new RandomAccessFile(archive,"r")) {
            byte[] magic=new byte[4];
            if(raw.read(magic)==4 && isRomMagic(magic)) {
                if(archive.length()<4L*1024*1024)throw new IOException("ROM is too small to be a Mario 64 image");
                return;
            }
        }
        boolean hasVersion=false,hasGameResources=false,hasMario64Rom=false;long total=0;int count=0;
        Set<String> names=new HashSet<>();
        try(ZipFile zip=new ZipFile(archive)) {
            Enumeration<? extends ZipEntry> entries=zip.entries();
            byte[] buffer=new byte[65536];
            while(entries.hasMoreElements()) {
                ZipEntry entry=entries.nextElement();String name=entry.getName();
                if(++count>100000)throw new IOException("Too many archive entries");
                if(name.startsWith("/")||name.contains("\\")||name.contains(":")||Arrays.asList(name.split("/")).contains("..")||!names.add(name))throw new IOException("Invalid or duplicate archive path");
                if(entry.isDirectory())continue;
                if(entry.getSize()<0||entry.getSize()>MAX_EXPANDED_BYTES)throw new IOException("Invalid entry size");
                CRC32 crc=new CRC32();long read=0;
                try(InputStream input=zip.getInputStream(entry)) {int n;while((n=input.read(buffer))!=-1){read+=n;total+=n;if(total>MAX_EXPANDED_BYTES)throw new IOException("Archive expansion limit exceeded");crc.update(buffer,0,n);}}
                if(read!=entry.getSize()||crc.getValue()!=entry.getCrc())throw new IOException("Archive checksum mismatch");
                if(name.equals("version"))hasVersion=true;
                if(name.startsWith("textures/")||name.startsWith("courses/")||name.startsWith("objects/"))hasGameResources=true;
                String lower=name.toLowerCase(Locale.ROOT);
                if(lower.endsWith(".z64")||lower.endsWith(".n64")||lower.endsWith(".v64"))hasMario64Rom=true;
            }
        }
        if(!((hasVersion&&hasGameResources)||hasMario64Rom))throw new IOException("ZIP must contain an MK64 O2R layout or a .z64, .n64, or .v64 Mario 64 ROM");
    }
    private static boolean isRomMagic(byte[] magic) {
        return (magic[0]&255)==0x80&&(magic[1]&255)==0x37&&(magic[2]&255)==0x12&&(magic[3]&255)==0x40
            ||(magic[0]&255)==0x40&&(magic[1]&255)==0x12&&(magic[2]&255)==0x37&&(magic[3]&255)==0x80
            ||(magic[0]&255)==0x37&&(magic[1]&255)==0x80&&(magic[2]&255)==0x40&&(magic[3]&255)==0x12;
    }
}
