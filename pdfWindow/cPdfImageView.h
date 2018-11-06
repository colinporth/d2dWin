// cPdfImageView.h
#include "cD2dWindow.h"

class cPdfImageView : public cD2dWindow::cView {
public:
  //{{{
  cPdfImageView (cD2dWindow* window, float width, float height, cPdfImage* image)
      : cView("image", window, width, height), mImage(image) {

    mPin = true;
    window->getDc()->CreateSolidColorBrush (ColorF (ColorF::Black), &mBrush);

    if (image && image->isOk())
      image->loadInfo();
    }
  //}}}
  //{{{
  cPdfImageView (cD2dWindow* window, float width, float height, bool tab, bool sideBar, cPdfImage* image)
      : cView("image", window, width, height), mImage(image) {

    mPin = true;
    window->getDc()->CreateSolidColorBrush (ColorF (ColorF::Black), &mBrush);

    if (image && image->isOk())
      image->loadInfo();
    }
  //}}}
  //{{{
  virtual ~cPdfImageView() {
    mBrush->Release();
    }
  //}}}

  //{{{
  void setImage (cPdfImage* image) {

    mImage = image;
    if (image) {
      image->loadInfo();
      setScale();
      }
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
        dc->SetTransform (Matrix3x2F::Identity());
        }
        //}}}
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

  cPdfImage* mImage;

  ID2D1SolidColorBrush* mBrush;
  cPoint mSamplePos;
  cPoint mPos;
  };
