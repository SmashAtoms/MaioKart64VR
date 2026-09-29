package org.aether64.xr;

import android.app.NativeActivity;
import android.app.AlertDialog;
import android.content.Intent;
import android.content.ActivityNotFoundException;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.net.Uri;
import android.os.Bundle;
import android.os.StatFs;
import java.io.*;
import java.nio.file.*;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class AetherActivity extends NativeActivity {
    static { System.loadLibrary("aether64"); }
    private static final int IMPORT_ARCHIVE=64;
    private final ExecutorService io=Executors.newSingleThreadExecutor();
    private volatile boolean importing=false;
    private volatile String importResult="";

    @Override public void onCreate(Bundle state) { super.onCreate(state); }
    @Override public void onDestroy() { io.shutdown(); super.onDestroy(); }

    // Called only when the user selects Import in the engine menu.
    public void requestImport() {
        runOnUiThread(() -> {
            if(importing) return;
            Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("*/*");
            try { startActivityForResult(intent,IMPORT_ARCHIVE); }
            catch(ActivityNotFoundException e) { importResult="No document picker is available on this Quest software. Install a compatible document provider, then retry."; }
        });
    }

    @Override protected void onActivityResult(int request,int result,Intent data) {
        super.onActivityResult(request,result,data);
        if(request!=IMPORT_ARCHIVE) return;
        if(result!=RESULT_OK||data==null||data.getData()==null) { importResult="Import cancelled. Existing data was kept.";return; }
        Uri uri=data.getData();importing=true;importResult="Importing and checking archive...";
        io.execute(() -> {
            File dir=new File(getFilesDir(),"games/mk64");
            File temporary=null;
            try {
                if(!dir.isDirectory()&&!dir.mkdirs())throw new IOException("Cannot create game storage");
                temporary=File.createTempFile("import-",".partial",dir);
                try(InputStream in=getContentResolver().openInputStream(uri);FileOutputStream out=new FileOutputStream(temporary)) {
                    if(in==null)throw new IOException("The selected document cannot be read");
                    byte[] buffer=new byte[65536];long total=0;int n;
                    while((n=in.read(buffer))!=-1) {
                        total+=n;
                        if(total>ArchiveValidator.MAX_ARCHIVE_BYTES)throw new IOException("Archive exceeds the supported 1 GiB limit");
                        if(new StatFs(dir.getAbsolutePath()).getAvailableBytes()<n+1048576L)throw new IOException("Not enough free storage");
                        out.write(buffer,0,n);
                    }
                    out.getFD().sync();
                }
                ArchiveValidator.validate(temporary);
                Files.move(temporary.toPath(),new File(dir,"mk64.o2r").toPath(),StandardCopyOption.ATOMIC_MOVE,StandardCopyOption.REPLACE_EXISTING);
                importResult="MK64 archive imported. The original document was not changed. A compatible game renderer is still required to launch it.";
            }catch(Exception e){importResult="Import failed: "+e.getMessage()+". Existing data was kept.";}
            finally {if(temporary!=null&&temporary.exists())temporary.delete();importing=false;}
        });
    }
    public synchronized String pollImportResult() { String r=importResult;importResult="";return r; }
    public String gameArchive() {return new File(getFilesDir(),"games/mk64/mk64.o2r").getAbsolutePath();}
    public boolean hasGameData() {return new File(gameArchive()).isFile();}
    public String dataDirectory() {return getFilesDir().getAbsolutePath();}
    public int loadCamera() {return getPreferences(MODE_PRIVATE).getInt("camera",0)==1?1:0;}
    public boolean loadDiagnostics() {return getPreferences(MODE_PRIVATE).getBoolean("diagnostics",false);}
    public void saveSettings(int camera,boolean diagnostics) {getPreferences(MODE_PRIVATE).edit().putInt("camera",camera).putBoolean("diagnostics",diagnostics).apply();}
    public void showError(String message) {runOnUiThread(() -> new AlertDialog.Builder(this).setTitle("Aether64 XR").setMessage(message).setPositiveButton("Close",(d,w)->finish()).show());}
    public void exportReport(String report) {
        io.execute(() -> {try {
            File file=new File(getExternalFilesDir(null),"aether64-diagnostics.txt");
            try(FileOutputStream out=new FileOutputStream(file)){out.write(report.getBytes(java.nio.charset.StandardCharsets.UTF_8));}
            importResult="Report saved to "+file.getAbsolutePath();
        }catch(Exception e){importResult="Report export failed: "+e.getMessage();}});
    }

    // Android's system typeface is rasterized here; no game font or bitmap is required.
    public Bitmap renderPanel(String title,String[] rows,int selected,String message) {
        Bitmap image=Bitmap.createBitmap(1024,1024,Bitmap.Config.ARGB_8888);
        Canvas canvas=new Canvas(image);canvas.drawColor(Color.rgb(12,19,32));
        Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(Color.rgb(66,219,197));canvas.drawRoundRect(48,48,120,120,18,18,paint);
        paint.setColor(Color.rgb(12,19,32));paint.setTypeface(Typeface.create("sans-serif",Typeface.BOLD));paint.setTextSize(30);canvas.drawText("64",66,96,paint);
        paint.setColor(Color.rgb(145,166,188));paint.setTextSize(22);canvas.drawText("AETHER / STANDALONE XR",148,93,paint);
        paint.setColor(Color.WHITE);paint.setTextSize(42);canvas.drawText(title,52,184,paint);
        float y=230;
        for(int i=0;i<rows.length;i++) {
            paint.setColor(i==selected?Color.rgb(35,91,92):Color.rgb(23,35,51));canvas.drawRoundRect(48,y,976,y+76,14,14,paint);
            paint.setColor(i==selected?Color.rgb(125,255,228):Color.rgb(220,232,244));paint.setTextSize(27);canvas.drawText(rows[i],76,y+49,paint);y+=90;
        }
        paint.setTypeface(Typeface.create("sans-serif",Typeface.NORMAL));paint.setColor(Color.rgb(164,184,204));paint.setTextSize(23);
        float lineY=y+20;
        for(String paragraph:message.split("\n")) {
            String line="";
            for(String word:paragraph.split(" ")) {
                if(paint.measureText(line+word)>900&&!line.isEmpty()){canvas.drawText(line,52,lineY,paint);lineY+=32;line="";}
                line+=word+" ";
            }
            if(lineY<939){canvas.drawText(line,52,lineY,paint);lineY+=32;}
        }
        paint.setColor(Color.rgb(66,219,197));paint.setTextSize(20);canvas.drawText("LEFT STICK  navigate     A  select     B  back     MENU  pause",52,975,paint);
        return image;
    }
}
