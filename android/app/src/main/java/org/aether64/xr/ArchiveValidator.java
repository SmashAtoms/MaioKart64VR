package org.aether64.xr;

import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

/** Installs either a raw N64 ROM or the single ROM contained in a ZIP. */
public final class ArchiveValidator {
    public static final long MAX_ARCHIVE_BYTES=1024L*1024*1024;
    private static final long MIN_ROM_BYTES=4L*1024*1024;
    private ArchiveValidator() {}

    public static void installRom(File source,File destination)throws IOException {
        if(source.length()<4||source.length()>MAX_ARCHIVE_BYTES)throw new IOException("Invalid file size");
        if(isRawRom(source)) {
            validateRawRom(source);
            Files.move(source.toPath(),destination.toPath(),StandardCopyOption.ATOMIC_MOVE,StandardCopyOption.REPLACE_EXISTING);
            return;
        }

        File extracted=File.createTempFile("rom-",".partial",destination.getParentFile());
        try(ZipFile zip=new ZipFile(source)) {
            ZipEntry rom=null;
            Enumeration<? extends ZipEntry> entries=zip.entries();
            Set<String> names=new HashSet<>();
            while(entries.hasMoreElements()) {
                ZipEntry entry=entries.nextElement();
                String name=entry.getName();
                if(name.startsWith("/")||name.contains("\\")||name.contains(":")||Arrays.asList(name.split("/")).contains("..")||!names.add(name))throw new IOException("Invalid or duplicate ZIP path");
                if(entry.isDirectory())continue;
                String lower=name.toLowerCase(Locale.ROOT);
                if(lower.endsWith(".z64")||lower.endsWith(".n64")||lower.endsWith(".v64")) {
                    if(rom!=null)throw new IOException("ZIP contains more than one ROM; keep only one .z64, .n64, or .v64 file");
                    rom=entry;
                }
            }
            if(rom==null)throw new IOException("ZIP does not contain a .z64, .n64, or .v64 ROM");
            if(rom.getSize()<MIN_ROM_BYTES||rom.getSize()>MAX_ARCHIVE_BYTES)throw new IOException("ROM has an invalid size");
            CRC32 crc=new CRC32();long total=0;byte[] buffer=new byte[65536];
            try(InputStream input=zip.getInputStream(rom);FileOutputStream output=new FileOutputStream(extracted)) {
                int n;
                while((n=input.read(buffer))!=-1) {
                    total+=n;
                    if(total>MAX_ARCHIVE_BYTES)throw new IOException("ROM exceeds the supported 1 GiB limit");
                    output.write(buffer,0,n);crc.update(buffer,0,n);
                }
                output.getFD().sync();
            }
            if(total!=rom.getSize()||crc.getValue()!=rom.getCrc())throw new IOException("ROM ZIP checksum mismatch");
            validateRawRom(extracted);
            Files.move(extracted.toPath(),destination.toPath(),StandardCopyOption.ATOMIC_MOVE,StandardCopyOption.REPLACE_EXISTING);
        } finally {
            if(extracted.exists())extracted.delete();
        }
    }

    private static void validateRawRom(File file)throws IOException {
        if(file.length()<MIN_ROM_BYTES||file.length()>MAX_ARCHIVE_BYTES)throw new IOException("ROM has an invalid size");
        if(!isRawRom(file))throw new IOException("ROM header is not a supported N64 byte order");
    }

    private static boolean isRawRom(File file)throws IOException {
        try(RandomAccessFile raw=new RandomAccessFile(file,"r")) {
            byte[] magic=new byte[4];
            return raw.read(magic)==4&&isRomMagic(magic);
        }
    }

    private static boolean isRomMagic(byte[] magic) {
        return (magic[0]&255)==0x80&&(magic[1]&255)==0x37&&(magic[2]&255)==0x12&&(magic[3]&255)==0x40
            ||(magic[0]&255)==0x40&&(magic[1]&255)==0x12&&(magic[2]&255)==0x37&&(magic[3]&255)==0x80
            ||(magic[0]&255)==0x37&&(magic[1]&255)==0x80&&(magic[2]&255)==0x40&&(magic[3]&255)==0x12;
    }
}
