package lyn.android_ffmpeg.tutorial;

import android.content.Context;
import android.content.res.TypedArray;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.util.Log;
import android.util.TypedValue;
import android.widget.SeekBar;

//public class LynSeekBar extends SeekBar
public class LynSeekBar extends SeekBar
{
    private static final int DEFAULT_TEXT_SIZE = 10;
    private static final int DEFAULT_TEXT_COLOR = 0XFFFC00D1;
    private static final int DEFAULT_COLOR_UNREACHED_COLOR = 0xFFd3d6da;
    private static final int DEFAULT_THUMB_COLOR = 0XFF00FFFF;
    private static final int DEFAULT_HEIGHT_REACHED_SEEK_BAR = 2;
    private static final int DEFAULT_HEIGHT_UNREACHED_SEEK_BAR = 2;
    private static final int DEFAULT_WIDTH_THUMB_SEEK_BAR = 4;
    private static final int DEFAULT_HEIGHT_THUMB_SEEK_BAR = 10;

    /**
     * painter of all drawing things
     */
    protected Paint mPaint = new Paint();
    /**
     * color of progress number
     */
    protected int mTextColor = DEFAULT_TEXT_COLOR;
    /**
     * size of text (sp)
     */
    protected int mTextSize = sp2px(DEFAULT_TEXT_SIZE);

    /**
     * height of reached seek bar
     */
    protected int mReachedSeekBarHeight = dp2px(DEFAULT_HEIGHT_REACHED_SEEK_BAR);

    /**
     * color of reached bar
     */
    protected int mReachedBarColor = DEFAULT_TEXT_COLOR;
    /**
     * color of unreached bar
     */
    protected int mUnReachedBarColor = DEFAULT_COLOR_UNREACHED_COLOR;
    /**
     * height of unreached seek bar
     */
    protected int mUnReachedSeekBarHeight = dp2px(DEFAULT_HEIGHT_UNREACHED_SEEK_BAR);
    /**
     * color of thumb seek bar
     */
    protected int mThumbColor = DEFAULT_THUMB_COLOR;
    /**
     * width of thumb seek bar
     */
    protected int mThumbSeekBarWidth = dp2px(DEFAULT_WIDTH_THUMB_SEEK_BAR);
    /**
     * width of thumb seek bar
     */
    protected int mThumbSeekBarHeight = dp2px(DEFAULT_HEIGHT_THUMB_SEEK_BAR);
    /**
     * view width except padding
     */
    protected int mRealWidth;

    protected boolean mIfDrawText = true;

    protected static final int VISIBLE = 0;

    public LynSeekBar(Context context, AttributeSet attrs)
    {
        this(context, attrs, 0);
    }

    public LynSeekBar(Context context, AttributeSet attrs,
                                           int defStyle)
    {
        super(context, attrs, defStyle);
        obtainStyledAttributes(attrs);
        mPaint.setTextSize(mTextSize);
        mPaint.setColor(mTextColor);
    }

    @Override
    protected synchronized void onMeasure(int widthMeasureSpec,
                                          int heightMeasureSpec)
    {
        int width = MeasureSpec.getSize(widthMeasureSpec);
        int height = measureHeight(heightMeasureSpec);
        setMeasuredDimension(width, height);

        mRealWidth = getMeasuredWidth() - getPaddingRight() - getPaddingLeft();
    }

    private int measureHeight(int measureSpec)
    {
        int result = 0;
        int specMode = MeasureSpec.getMode(measureSpec);
        int specSize = MeasureSpec.getSize(measureSpec);
        if (specMode == MeasureSpec.EXACTLY)
        {
            result = specSize;
        } else
        {
            float textHeight = (mPaint.descent() - mPaint.ascent());
            result = (int) (getPaddingTop() + getPaddingBottom() + Math.max(
                    Math.max(mReachedSeekBarHeight,
                            mUnReachedSeekBarHeight), Math.abs(textHeight)));
            if (specMode == MeasureSpec.AT_MOST)
            {
                result = Math.min(result, specSize);
            }
        }
        return result;
    }

    /**
     * get the styled attributes
     *
     * @param attrs
     */
    private void obtainStyledAttributes(AttributeSet attrs)
    {
        // init values from custom attributes
        final TypedArray attributes = getContext().obtainStyledAttributes(
                attrs, R.styleable.LynSeekBar);

        mTextColor = attributes
                .getColor(
                        R.styleable.LynSeekBar_seek_text_color,
                        DEFAULT_TEXT_COLOR);
        mTextSize = (int) attributes.getDimension(
                R.styleable.LynSeekBar_seek_text_size,
                mTextSize);

        mReachedBarColor = attributes
                .getColor(
                        R.styleable.LynSeekBar_seek_reached_color,
                        mTextColor);
        mUnReachedBarColor = attributes
                .getColor(
                        R.styleable.LynSeekBar_seek_unreached_color,
                        DEFAULT_COLOR_UNREACHED_COLOR);
        mReachedSeekBarHeight = (int) attributes
                .getDimension(
                        R.styleable.LynSeekBar_seek_reached_bar_height,
                        mReachedSeekBarHeight);
        mUnReachedSeekBarHeight = (int) attributes
                .getDimension(
                        R.styleable.LynSeekBar_seek_unreached_bar_height,
                        mUnReachedSeekBarHeight);
        int textVisible = attributes
                .getInt(R.styleable.LynSeekBar_seek_text_visibility,
                        VISIBLE);
        if (textVisible != VISIBLE)
        {
            mIfDrawText = false;
        }
        attributes.recycle();
    }

    public static String secToTime(int time) {
        String timeStr = null;
        int hour = 0;
        int minute = 0;
        int second = 0;
        if (time <= 0)
            return "00:00";
        else {
            minute = time / 60;
            if (minute < 60) {
                second = time % 60;
                timeStr = unitFormat(minute) + ":" + unitFormat(second);
            } else {
                hour = minute / 60;
                if (hour > 99)
                    return "99:59:59";
                minute = minute % 60;
                second = time - hour * 3600 - minute * 60;
                timeStr = unitFormat(hour) + ":" + unitFormat(minute) + ":" + unitFormat(second);
            }
        }
        return timeStr;
    }

    public static String unitFormat(int i) {
        String retStr = null;
        if (i >= 0 && i < 10)
            retStr = "0" + Integer.toString(i);
        else
            retStr = "" + i;
        return retStr;
    }

    @Override
    protected synchronized void onDraw(Canvas canvas)
    {
        canvas.save();
        canvas.translate(getPaddingLeft(), getHeight() / 2);

        float radio = getProgress() * 1.0f / getMax();
        float progressPosX = (int) (mRealWidth * radio);
        String text = secToTime(getProgress());
        // mPaint.getTextBounds(text, 0, text.length(), mTextBound);

        float textWidth = mPaint.measureText(text);
        float textHeight = (mPaint.descent() + mPaint.ascent()) / 2;
        float textHeightPos = textHeight + mPaint.ascent();

        // draw reached bar
        float endX = progressPosX;
        if (endX > 0)
        {
            mPaint.setColor(mReachedBarColor);
            mPaint.setStrokeWidth(mReachedSeekBarHeight);
            canvas.drawLine(0, 0, endX, 0, mPaint);
        }

        // draw thumb
        mPaint.setColor(mThumbColor);
        mPaint.setStrokeWidth(mThumbSeekBarWidth);
        canvas.drawLine(endX, -mThumbSeekBarHeight/2, endX, mThumbSeekBarHeight/2, mPaint);

        // draw unreached bar
        float start = progressPosX;
        mPaint.setColor(mUnReachedBarColor);
        mPaint.setStrokeWidth(mUnReachedSeekBarHeight);
        canvas.drawLine(start, 0, mRealWidth, 0, mPaint);

        // draw text
        if (mIfDrawText) {
            mPaint.setColor(mTextColor);
            String endText = secToTime(getMax());
            float endTextWidth = mPaint.measureText(endText);
            float delimitWidth = mPaint.measureText("/");
            if (progressPosX + endTextWidth + delimitWidth >= mRealWidth){
                endText = text + "/" + endText;
                endTextWidth = mPaint.measureText(endText);
            } else {
                mPaint.setColor(mTextColor);
                float realPosX = progressPosX - textWidth + mThumbSeekBarWidth/2;
                canvas.drawText(text, realPosX > 0 ? realPosX : 0, -textHeightPos, mPaint);

            }
            canvas.drawText(endText, mRealWidth - endTextWidth, -textHeightPos, mPaint);
        }

        canvas.restore();

    }

    /**
     * dp 2 px
     *
     * @param dpVal
     */
    protected int dp2px(int dpVal)
    {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP,
                dpVal, getResources().getDisplayMetrics());
    }

    /**
     * sp 2 px
     *
     * @param spVal
     * @return
     */
    protected int sp2px(int spVal)
    {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP,
                spVal, getResources().getDisplayMetrics());

    }

}
