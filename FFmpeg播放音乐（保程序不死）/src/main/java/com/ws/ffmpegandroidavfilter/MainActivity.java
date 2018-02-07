package com.ws.ffmpegandroidavfilter;
import android.Manifest;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Build;
import android.os.Bundle;
import android.support.v7.app.AppCompatActivity;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.widget.Button;


public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("native-lib");
    }

    SurfaceHolder surfaceHolder;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Button btn = (Button)findViewById(R.id.btn);

        btn.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                MainActivity.this.playSound("/storage/emulated/0/pauseRecordDemo/video/a.mp3");
            }
        });

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            requestPermissions(new String[]{Manifest.permission.WRITE_EXTERNAL_STORAGE}, 5);
        }
    }




    public native int play(Object surface);




    public native void playSound(String input);

    private AudioTrack audioTrack;
    //    这个方法  是C进行调用  通道数
    public void createTrack(int sampleRateInHz,int nb_channals) {

        int channaleConfig;//通道数
        if (nb_channals == 1) {
            channaleConfig = AudioFormat.CHANNEL_OUT_MONO;
        } else if (nb_channals == 2) {
            channaleConfig = AudioFormat.CHANNEL_OUT_STEREO;
        }else {
            channaleConfig = AudioFormat.CHANNEL_OUT_MONO;
        }
        int buffersize=AudioTrack.getMinBufferSize(sampleRateInHz,
                channaleConfig, AudioFormat.ENCODING_PCM_16BIT);
        audioTrack = new AudioTrack(AudioManager.STREAM_MUSIC,sampleRateInHz,channaleConfig,
                AudioFormat.ENCODING_PCM_16BIT,buffersize,AudioTrack.MODE_STREAM);
        audioTrack.play();
    }

    //C传入音频数据
    public void playTrack(byte[] buffer, int lenth) {
        if (audioTrack != null && audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) {
            audioTrack.write(buffer, 0, lenth);
        }
    }
}
