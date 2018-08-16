package com.ywl5320.opengles;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;

import com.ywl5320.listener.WlOnGlSurfaceViewOncreateListener;
import com.ywl5320.listener.WlOnRenderRefreshListener;


public class ZXGlSurfaceView extends GLSurfaceView{

    private WlGlRender wlGlRender;
    private WlOnGlSurfaceViewOncreateListener onGlSurfaceViewOncreateListener;

    public ZXGlSurfaceView(Context context) {
        this(context, null);
    }

    public ZXGlSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        wlGlRender = new WlGlRender(context);
        //设置egl版本为2.0
        setEGLContextClientVersion(2);
        //设置render
        setRenderer(wlGlRender);
        //设置为手动刷新模式
        setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);
        wlGlRender.setWlOnRenderRefreshListener(new WlOnRenderRefreshListener() {
            @Override
            public void onRefresh() {
                requestRender();
            }
        });
    }

    public void setOnGlSurfaceViewOncreateListener(WlOnGlSurfaceViewOncreateListener onGlSurfaceViewOncreateListener) {
        if(wlGlRender != null)
        {
            wlGlRender.setWlOnGlSurfaceViewOncreateListener(onGlSurfaceViewOncreateListener);
        }
    }

    public void setCodecType(int type)
    {
        if(wlGlRender != null)
        {
            wlGlRender.setCodecType(type);
        }
    }


    public void setFrameData(int w, int h, byte[] y, byte[] u, byte[] v)
    {
        if(wlGlRender != null)
        {
            wlGlRender.setFrameData(w, h, y, u, v);
            requestRender();
        }
    }

    public void cutVideoImg()
    {
        if(wlGlRender != null)
        {
            wlGlRender.cutVideoImg();
            requestRender();
        }
    }
}
