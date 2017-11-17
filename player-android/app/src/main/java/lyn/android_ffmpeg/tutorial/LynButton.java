package lyn.android_ffmpeg.tutorial;

import android.graphics.Canvas;
import android.graphics.Paint;
import android.util.TypedValue;
import android.widget.Button;
import android.util.AttributeSet;
import android.content.Context;

public class LynButton extends Button {
    private static final int DEFAULT_BUTTON_LINE_COLOR = 0XFF1E90FF;
    private static final int DEFAULT_BUTTON_LINE_WIDTH = 1;

    /**
     * color of button line
     */
    protected int mButtonLineColor = DEFAULT_BUTTON_LINE_COLOR;
    /**
     * size of button line (sp)
     */
    protected int mButtonLineWidth = sp2px(DEFAULT_BUTTON_LINE_WIDTH);

    private Paint mPaint;

    private Boolean mIsPlay = false;

    public LynButton(Context context, AttributeSet attrs) {
        super(context, attrs);
        mPaint = new Paint();
    }

    public void changeIsPlay() {
        mIsPlay = !mIsPlay;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        float width = getMeasuredWidth();
        float height = getMeasuredHeight();
        mPaint.setColor(mButtonLineColor);
        mPaint.setStrokeWidth(mButtonLineWidth);
        if(mIsPlay == false) {
            float heightStep = height / 4;
            float widthLen = heightStep * (float) 1.732;
            float widthStep = (width - widthLen) / 2;
            float pts[] = {widthStep, heightStep, widthStep, heightStep * 3,
                    widthStep, heightStep, widthLen + widthStep, heightStep * 2,
                    widthStep, heightStep * 3, widthLen + widthStep, heightStep * 2};
            canvas.drawLines(pts,mPaint);
        } else {
            float heightStep = height / 4;
            float widthLen = heightStep;
            float widthStep = (width - widthLen) / 2;
            float pts[] = {widthStep, heightStep, widthStep, heightStep * 3,
                    widthLen + widthStep, heightStep, widthLen + widthStep, heightStep * 3};
            canvas.drawLines(pts,mPaint);
        }
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

