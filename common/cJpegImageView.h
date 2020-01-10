// cJpegImageView.cpp
#include "cD2dWindow.h"

class cJpegImageView : public cD2dWindow::cView {
public:
  //{{{
  cJpegImageView (cD2dWindow* window, float width, float height, cJpegImage* image)
      : cView("image", window, width, height), mImage(image), mTab(true), mSideBar(true) {

    mPin = true;
    window->getDc()->CreateSolidColorBrush (D2D1::ColorF (D2D1::ColorF::Black), &mBrush);

    if (image && image->isOk())
      image->loadInfo();
    }
  //}}}
  //{{{
  cJpegImageView (cD2dWindow* window, float width, float height, bool tab, bool sideBar, cJpegImage* image)
      : cView("image", window, width, height), mImage(image), mTab(tab), mSideBar(sideBar) {

    mPin = true;
    window->getDc()->CreateSolidColorBrush (D2D1::ColorF (D2D1::ColorF::Black), &mBrush);

    if (image && image->isOk())
      image->loadInfo();
    }
  //}}}
  //{{{
  virtual ~cJpegImageView() {
    mBrush->Release();
    }
  //}}}

  //{{{
  void setImage (cJpegImage* image) {

    mImage = image;
    if (image) {
      image->loadInfo();
      setScale();
      }
    }
  //}}}
  //{{{
  void setTab (bool tab) {
    mTab = tab;
    }
  //}}}
  //{{{
  void setSideBar (bool sideBar) {
    mSideBar = sideBar;
    }
  //}}}

  // overrides
  //{{{
  cPoint getSrcSize() {
    return (mImage && mImage->isLoaded()) ? mImage->getSize() : getSize();
    }
  //}}}
  //{{{
  void layout() {
    cView::layout();
    mView2d.setPos (getCentre());
    }
  //}}}
  //{{{
  bool onProx (bool inClient, cPoint pos) {

    mPos = pos;
    mSamplePos = mView2d.getDstToSrc (pos);
    mWindow->changed();

    return cView::onProx (inClient, pos);
    }
  //}}}
  //{{{
  void onDraw (ID2D1DeviceContext* dc) {

    if (mImage && mImage->isOk()) {
      setScale();

      // needs refreshing after load
      auto dstRect = mView2d.getSrcToDst (cRect(getSrcSize()));
      if (mImage->getBitmap()) {
        //{{{  draw bitmap
        dc->SetTransform (mView2d.mTransform);
        dc->DrawBitmap (mImage->getBitmap(), cRect (mImage->getSize()));
        dc->DrawRectangle (cRect (mImage->getSize()), mWindow->getWhiteBrush());
        dc->SetTransform (D2D1::Matrix3x2F::Identity());
        }
        //}}}

      if (mTab)
        drawTab (dc, mImage->getFileName() + " - scale " + dec(mImage->getScale()) + " " + dec(mView2d.getScale()),
                 dstRect, mWindow->getLightGreyBrush());

      if (mSideBar) {
        // draw sidebar
        auto r = cRect (dstRect.getTR(), dstRect.getTR() + cPoint(240.f, kLineHeight));
        //{{{  clip to mWindow
        if (r.top - kLineHeight < 0) {
          r.top = 0;
          r.bottom = r.top + kLineHeight;
          }
        if (r.right > mWindow->getWidth()) {
          r.right = mWindow->getWidth();
          r.left = r.right - 240.0f;
          }
        //}}}

        drawDebug (dc, dec(mPos.x) + " " + dec(mPos.y), r);
        drawDebug (dc, dec(mSamplePos.x) + " " + dec(mSamplePos.y), r);
        //{{{  draw image width, height
        drawDebug (dc, dec(mImage->getImageWidth()) + "x" + dec(mImage->getImageHeight()) +
                       " @ 1/" + dec(mImage->getScale()) + " " +
                       dec(mImage->getWidth()) + "x" + dec(mImage->getHeight()), r);
        //}}}
        //{{{  draw image info
        drawDebug (dc, "image " + mImage->getDebugString(), r);
        //}}}
        //{{{  draw thumbnail info
        if (!mImage->getThumbString().empty())
          drawDebug(dc, "thumb " + mImage->getThumbString(), r);
        //}}}
        //{{{  draw times
        if (!mImage->getExifTimeString().empty())
          drawDebug (dc, "e " + mImage->getExifTimeString(), r);
        if (!mImage->getCreationTimeString().empty())
          drawDebug (dc, "c " + mImage->getCreationTimeString(), r);
        if (!mImage->getLastAccessTimeString().empty())
          drawDebug (dc, "a " + mImage->getLastAccessTimeString(), r);
        if (!mImage->getLastWriteTimeString().empty())
          drawDebug (dc, "w " + mImage->getLastWriteTimeString(), r);
        //}}}
        //{{{  draw make, model, gps
        if (!mImage->getMakeString().empty())
          drawDebug (dc, mImage->getMakeString(), r);
        if (!mImage->getModelString().empty())
          drawDebug (dc, mImage->getModelString(), r);
        if (!mImage->getGpsString().empty())
          drawDebug (dc, mImage->getGpsString(), r);
        //}}}
        //{{{  draw orient, aperture, focal length, exposure
        if (mImage->getOrientation())
          drawDebug (dc, "orient ", mImage->getOrientation(), r);
        if (mImage->getAperture())
          drawDebug (dc, "aperture " + dec(mImage->getAperture()), r);
        if (mImage->getFocalLength())
          drawDebug (dc, "focal length " + dec(mImage->getFocalLength()), r);
        if (mImage->getExposure())
          drawDebug (dc, "exposure " + dec(mImage->getExposure()), r);
        //}}}
        //{{{  draw jpeg markers
        if (!mImage->getMarkerString().empty()) {
          r.bottom += 2.f * kLineHeight;
          drawDebug (dc, mImage->getMarkerString(), r);
          }
        //}}}
        if (mImage->getBgraBuf()) {
          //{{{  draw pick colour
          int red = mImage->getRed (mSamplePos);
          int green = mImage->getGreen (mSamplePos);
          int blue = mImage->getBlue (mSamplePos);
          mBrush->SetColor (D2D1::ColorF(red /255.f, green /255.f, blue /255.f, 1.f));
          drawDebug (dc, "r"+dec(red) + " g" + dec(green) + " b" + dec(blue), mBrush, r);
          }
          //}}}
        }
      }
    }
  //}}}

private:
  //{{{
  void setScale () {

    auto dstRect = mView2d.getSrcToDst (cRect(getSrcSize()));
    int scale = 1 + int(mImage->getImageSize().x / dstRect.getWidth());
    if (!mImage->isLoaded() || (scale != mImage->getScale())) {
      cLog::log (LOGNOTICE, "loadImage new:%d old:%d", scale, mImage->getScale());
      mImage->loadImage (mWindow->getDc(), scale);
      }

    auto srcScaleX = getSize().x / getSrcSize().x;
    auto srcScaleY = getSize().y / getSrcSize().y;
    auto bestScale = (srcScaleX < srcScaleY) ? srcScaleX : srcScaleY;
    mView2d.setSrcScale (bestScale);
    mView2d.setSrcPos (getSrcSize() * bestScale / -2.f);
    }
  //}}}

  cJpegImage* mImage;
  bool mTab;
  bool mSideBar;

  ID2D1SolidColorBrush* mBrush;
  cPoint mSamplePos;
  cPoint mPos;
  };
