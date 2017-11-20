package lyn.android_ffmpeg.tutorial;

import java.io.File;

import android.content.res.Configuration;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.Handler;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Point;
import android.os.Message;
import android.util.Log;
import android.view.Display;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;
import android.widget.RelativeLayout;
import android.widget.RelativeLayout.LayoutParams;
import android.media.AudioTrack;
import android.media.AudioFormat;
import java.lang.ref.WeakReference;
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;

public class MainActivity extends Activity implements SurfaceHolder.Callback {
	private static final String TAG = "android-ffmpeg-tutorial07";
	private static final String FRAME_DUMP_FOLDER_PATH = Environment.getExternalStorageDirectory() 
			+ File.separator + "android-ffmpeg-tutorial07";
	
	// video used to fill the width of the screen 
	private static final String videoFileName = "1.mp4";  	//304x544
	// video used to fill the height of the screen
	//private static final String videoFileName = "2.mp4";   //480x208
	//private static final String videoFileName = "1.flv";   //640x360

	private SurfaceView mSurfaceView;
	private AudioTrack audioTrack;
	private int samplerate,channeltype,sampleformat,minbufsize;
	private boolean mUseAudioTrack = false;
	public volatile boolean mThreadState = true;
	private boolean isPlayStart = false;
	private int isPaused = 0;
	private float Ratio;
	private LynSeekBar mLynSeekBar;
	private LynButton mLynButtonStart;
	private static final int MSG_PROGRESS_UPDATE = 0x110;

	private static class LynHandler extends Handler{
		private final WeakReference<MainActivity> mActivity;

		public LynHandler(MainActivity activity){
			mActivity = new WeakReference<MainActivity>(activity);
		}

		@Override
		public void handleMessage(Message msg) {
			MainActivity activity = mActivity.get();
			if(activity==null){
				super.handleMessage(msg);
				return;
			}
			switch (msg.what) {
				case MSG_PROGRESS_UPDATE:
					activity.mLynSeekBar.setProgress(Integer.parseInt((String)msg.obj));
					break;
				default:
					super.handleMessage(msg);
					break;
			}
		}
	}
	private final LynHandler mHandler = new LynHandler(this);

	private Thread audioUpdateThread = new Thread(){
		public void run() {
			while(mThreadState) {
				byte[] pcm = new byte[minbufsize];
				int dsize = naGetPcmBuffer(pcm, minbufsize);
				if (audioTrack.write(pcm, 0, dsize) < dsize) {
					Log.w(null, "Data not written completely");
				}
			}
		}
	};
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
            WindowManager.LayoutParams.FLAG_FULLSCREEN);
		setContentView(R.layout.activity_main);
		//create directory for the tutorial
		File dumpFolder = new File(FRAME_DUMP_FOLDER_PATH);
		if (!dumpFolder.exists()) {
			dumpFolder.mkdirs();
		}
		//copy input video file from assets folder to directory
		Utils.copyAssets(this, videoFileName, FRAME_DUMP_FOLDER_PATH);
		naInit(FRAME_DUMP_FOLDER_PATH + File.separator + videoFileName);
		mSurfaceView = (SurfaceView)findViewById(R.id.surfaceview);
		mSurfaceView.getHolder().addCallback(this);
		mLynSeekBar = (LynSeekBar) findViewById(R.id.id_seekbar);
		mLynSeekBar.setOnSeekBarChangeListener(new OnSeekBarChangeListener() {
			@Override
			public void onProgressChanged(SeekBar seekBar, int i, boolean b) {

			}

			@Override
			public void onStartTrackingTouch(SeekBar seekBar) {

			}

			@Override
			public void onStopTrackingTouch(SeekBar seekBar) {

			}
		});

		mLynButtonStart = (LynButton) findViewById(R.id.buttonStart);
		mLynButtonStart.setOnClickListener(new View.OnClickListener() {
			@Override
			public void onClick(View v) {
				mLynButtonStart.changeIsPlay();
				mLynButtonStart.invalidate();
				if(isPlayStart == false) {
					if (mUseAudioTrack) {
						audioTrack.play();
						audioUpdateThread.start();
					}
					naPlay();
					isPlayStart = true;
				} else {
					isPaused = (isPaused == 0 ? 1 : 0);
					naPause(isPaused);
				}
			}
		});
	}
	
	private void updateSurfaceView(int pWidth, int pHeight) {
		//update surfaceview dimension, this will cause the native window to change
		RelativeLayout.LayoutParams params = (LayoutParams) mSurfaceView.getLayoutParams();
		params.width = pWidth;
		params.height = pHeight;
		mSurfaceView.setLayoutParams(params);
	}
	
	@SuppressLint("NewApi")
	private int[] getScreenRes() {
		int[] res = new int[2];
		Display display = getWindowManager().getDefaultDisplay();
		if (Build.VERSION.SDK_INT >= 13) {
			Point size = new Point();
			display.getSize(size);
			res[0] = size.x;
			res[1] = size.y;
		} else {
			res[0] = display.getWidth();  // deprecated
			res[1] = display.getHeight();  // deprecated
		}
		return res;
	}

	private void setSurfaceSize(){
		int[] res = naGetVideoRes();
		Log.d(TAG, "res width " + res[0] + ": height " + res[1]);
		int[] screenRes = getScreenRes();
		Log.d(TAG, "screenRes width " + screenRes[0] + ": height " + screenRes[1]);
		int width, height;
		float widthScaledRatio = screenRes[0]*1.0f/res[0];
		float heightScaledRatio = screenRes[1]*1.0f/res[1];
		Ratio = (float)res[0]/(float)res[1];
		if (widthScaledRatio > heightScaledRatio) {
			//use heightScaledRatio
			width = (int) (res[0]*heightScaledRatio);
			height = screenRes[1];
		} else {
			//use widthScaledRatio
			width = screenRes[0];
			height = (int) (res[1]*widthScaledRatio);
		}
		Log.d(TAG, "width " + width + ",height:" + height);
		updateSurfaceView(width, height);
		mLynSeekBar.setMax(res[2]);
	}

	@Override
	public void onConfigurationChanged(Configuration newConfig) {
		super.onConfigurationChanged(newConfig);
		Log.d(TAG, "onConfigurationChanged");
		setSurfaceSize();
	}

	@Override
	public void onDestroy() {
		Log.d(TAG, "onDestroy");
		super.onDestroy();
		System.exit(0);
	}

	@Override
	public void surfaceChanged(SurfaceHolder holder, int format, int width,
			int height) {
		Log.d(TAG, "surfacechanged: " + width + ":" + height);
		if((float)(Math.round(((float)width/(float)height)*100))/100 == (float)(Math.round(Ratio*100))/100) {
			naSetup(holder.getSurface(), width, height);
		}else{
			Log.d(TAG, "Ratio: " + Ratio);
		}
	}

	@Override
	public void surfaceCreated(SurfaceHolder holder) {
		Log.d(TAG, "surfaceCreated");
		setSurfaceSize();
		if(mUseAudioTrack) {
			int[] info = naGetAudioInfo();
			Log.d(TAG, "info rate " + info[0] + ": channels " + info[1] + ": fmt " + info[2]);
			samplerate = info[0];
			if (info[1] == 2) { //AV_CH_LAYOUT_STEREO
				channeltype = AudioFormat.CHANNEL_OUT_STEREO;
			} else { //AV_CH_LAYOUT_MONO and other
				channeltype = AudioFormat.CHANNEL_OUT_MONO;
			}
			sampleformat = AudioFormat.ENCODING_PCM_16BIT;
			minbufsize = AudioTrack.getMinBufferSize(samplerate, channeltype, sampleformat);
			audioTrack = new AudioTrack(android.media.AudioManager.STREAM_MUSIC, samplerate, channeltype,
					sampleformat, minbufsize * 2, AudioTrack.MODE_STREAM);
		}
	}

	@Override
	public void surfaceDestroyed(SurfaceHolder holder) {
		Log.d(TAG, "surfaceDestroyed");
		mThreadState = false;
		naSetup(null, 0, 0);
	}

	public void setNowTime(String time) {
		Message msg = Message.obtain();
		msg.obj = time;
		msg.what = MSG_PROGRESS_UPDATE;
		mHandler.sendMessage(msg);
	}

	private static native int naInit(String pFileName); 
	private static native int[] naGetVideoRes();
	private static native int[] naGetAudioInfo();
	private static native int naGetPcmBuffer(byte[] pcm,int len);
	private static native int naSetup(Surface pSurface,int pWidth, int pHeight);
	private native void naPlay();
	private static native void naPause(int pause);

	static {
		System.loadLibrary("avutil");
		System.loadLibrary("avcodec");
		System.loadLibrary("avformat");
		System.loadLibrary("swresample");
		System.loadLibrary("swscale");
		System.loadLibrary("tutorial");
    }
}
