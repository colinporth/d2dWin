// simple non templated channel contiguous interleaved float 32bit samples
#pragma once
//{{{  includes
#define NOMINMAX

#include <optional>
#include <cassert>
#include <chrono>
#include <cctype>
#include <codecvt>

#include <string>
#include <iostream>
#include <vector>

#include <functional>
#include <thread>
#include <forward_list>
#include <atomic>
#include <string_view>

#include <initguid.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <Functiondiscoverykeys_devpkey.h>

#include <variant>
#include <array>
//}}}

// cAudioBuffer
//{{{
class cAudioBuffer {
public:
  cAudioBuffer() {}

  cAudioBuffer (float* data, int numSamples, int numChannels)
      : mNumSamples(numSamples), mNumChannels(numChannels), mStride(mNumChannels) {

    assert (numChannels <= mMaxNumChannels);

    // stride and channels pointers for channel interleaved contiguous float 32bit samples
    for (auto i = 0; i < mNumChannels; ++i)
      mChannels[i] = data + i;
    }

  int getNumChannels() const noexcept { return mNumChannels; }
  int getSampleBytes() const noexcept { return mNumChannels * sizeof(float); }
  int getNumSamples() const noexcept { return mNumSamples; }

  float* getData() const noexcept { return mChannels[0]; }

  float& operator() (size_t frame, size_t channel) noexcept {
    return const_cast<float&>(std::as_const(*this).operator()(frame, channel));
    }

  const float& operator() (size_t frame, size_t channel) const noexcept {
    return mChannels[channel][frame * mStride];
    }

private:
  constexpr static int mMaxNumChannels = 6;

  int mNumChannels = 0;
  int mNumSamples = 0;
  int mStride = 0;

  std::array <float*, mMaxNumChannels> mChannels = {};
  };
//}}}

// cAudioDevice
//{{{
class cWaspiUtil {
public:
  //{{{
  static const CLSID& getMMDeviceEnumeratorClassId() {

    static const CLSID MMDeviceEnumerator_class_id = __uuidof(MMDeviceEnumerator);
    return MMDeviceEnumerator_class_id;
    }
  //}}}
  //{{{
  static const IID& getIMMDeviceEnumeratorInterfaceId() {

    static const IID IMMDeviceEnumerator_interface_id = __uuidof(IMMDeviceEnumerator);
    return IMMDeviceEnumerator_interface_id;
    }
  //}}}

  //{{{
  static const IID& getIAudioClientInterfaceId() {

    static const IID IAudioClient_interface_id = __uuidof(IAudioClient);
    return IAudioClient_interface_id;
    }
  //}}}
  //{{{
  static const IID& getIAudioRenderClientInterfaceId() {

    static const IID IAudioRenderClient_interface_id = __uuidof(IAudioRenderClient);
    return IAudioRenderClient_interface_id;
    }
  //}}}
  //{{{
  static const IID& getIAudioCaptureClientInterfaceId() {

    static const IID IAudioCaptureClient_interface_id = __uuidof(IAudioCaptureClient);
    return IAudioCaptureClient_interface_id;
    }
  //}}}

  //{{{
  class cComInitializer {
  public:
    cComInitializer() : mHr (CoInitialize (nullptr)) {}

    ~cComInitializer() {
      if (SUCCEEDED (mHr))
        CoUninitialize();
      }

    operator HRESULT() const { return mHr; }

    HRESULT mHr;
    };
  //}}}
  //{{{
  template<typename T> class cAutoRelease {
  public:
    cAutoRelease (T*& value) : _value(value) {}

    ~cAutoRelease() {
      if (_value != nullptr)
        _value->Release();
      }

  private:
    T*& _value;
    };
  //}}}

  //{{{
  static std::string convertString (const wchar_t* wide_string) {

    int required_characters = WideCharToMultiByte (CP_UTF8, 0, wide_string,
                                                   -1, nullptr, 0, nullptr, nullptr);
    if (required_characters <= 0)
      return {};

    std::string output;
    output.resize (static_cast<size_t>(required_characters));
    WideCharToMultiByte (CP_UTF8, 0, wide_string, -1,
                         output.data(), static_cast<int>(output.size()), nullptr, nullptr);

    return output;
    }
  //}}}
  //{{{
  static std::string convertString (const std::wstring& input) {

    int required_characters = WideCharToMultiByte (CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()),
                                                   nullptr, 0, nullptr, nullptr);
    if (required_characters <= 0)
      return {};

    std::string output;
    output.resize (static_cast<size_t>(required_characters));
    WideCharToMultiByte (CP_UTF8, 0, input.c_str(), static_cast<int>(input.size()),
                         output.data(), static_cast<int>(output.size()), nullptr, nullptr);

    return output;
    }
  //}}}
  };
//}}}
//{{{
struct sAudioDeviceException : public std::runtime_error {
  explicit sAudioDeviceException (const char* what) : runtime_error(what) { }
  };
//}}}
//{{{
class cAudioDevice {
public:
  cAudioDevice() = delete;
  cAudioDevice (const cAudioDevice&) = delete;
  cAudioDevice& operator= (const cAudioDevice&) = delete;

  //{{{
  cAudioDevice (cAudioDevice&& other) :
      mDevice(other.mDevice),
      mAudioClient(other.mAudioClient),
      mAudioCaptureClient(other.mAudioCaptureClient),
      mAudioRenderClient(other.mAudioRenderClient),
      mEventHandle(other.mEventHandle),
      mDeviceId(std::move(other.mDeviceId)),
      mName(std::move(other.mName)),
      mMixFormat(other.mMixFormat),
      mNumBufferSamples(other.mNumBufferSamples),
      mIsRenderDevice(other.mIsRenderDevice),
      mStopCallback(std::move(other.mStopCallback)),
      mUserCallback(std::move(other.mUserCallback)) {

    other.mDevice = nullptr;
    other.mAudioClient = nullptr;
    other.mAudioCaptureClient = nullptr;
    other.mAudioRenderClient = nullptr;
    other.mEventHandle = nullptr;
    }
  //}}}
  //{{{
  cAudioDevice& operator= (cAudioDevice&& other) noexcept {

    if (this == &other)
      return *this;

    mDevice = other.mDevice;
    mAudioClient = other.mAudioClient;
    mAudioCaptureClient = other.mAudioCaptureClient;
    mAudioRenderClient = other.mAudioRenderClient;
    mEventHandle = other.mEventHandle;
    mDeviceId = std::move(other.mDeviceId);
    mName = std::move(other.mName);
    mMixFormat = other.mMixFormat;
    mNumBufferSamples = other.mNumBufferSamples;
    mIsRenderDevice = other.mIsRenderDevice;
    mStopCallback = std::move (other.mStopCallback);
    mUserCallback = std::move (other.mUserCallback);

    other.mDevice = nullptr;
    other.mAudioClient = nullptr;
    other.mAudioCaptureClient = nullptr;
    other.mAudioRenderClient = nullptr;
    other.mEventHandle = nullptr;
    }
  //}}}
  //{{{
  ~cAudioDevice() {

    stop();

    if (mAudioCaptureClient != nullptr)
      mAudioCaptureClient->Release();

    if (mAudioRenderClient != nullptr)
      mAudioRenderClient->Release();

    if (mAudioClient != nullptr)
      mAudioClient->Release();

    if (mDevice != nullptr)
      mDevice->Release();
    }
  //}}}

  std::string getName() const noexcept { return mName; }
  std::wstring getDeviceId() const noexcept { return mDeviceId; }

  bool isInput() const noexcept { return !mIsRenderDevice; }
  bool isOutput() const noexcept { return mIsRenderDevice; }

  int getNumInputChannels() const noexcept { return isInput() ? mMixFormat.Format.nChannels : 0; }
  int getNumOutputChannels() const noexcept { return isOutput() ? mMixFormat.Format.nChannels : 0; }

  DWORD getSampleRate() const noexcept { return mMixFormat.Format.nSamplesPerSec; }
  UINT32 getNumBufferSamples() const noexcept { return mNumBufferSamples; }

  //{{{
  bool hasUnprocessedIo() const noexcept {

    if (mAudioClient == nullptr)
      return false;

    UINT32 currentPadding = 0;
    mAudioClient->GetCurrentPadding (&currentPadding);

    auto numSamplesAvailable = mNumBufferSamples - currentPadding;
    return numSamplesAvailable > 0;
    }
  //}}}

  //{{{
  bool setSampleRate (DWORD sampleRate) {

    mMixFormat.Format.nSamplesPerSec = sampleRate;
    mMixFormat.Format.nBlockAlign = mMixFormat.Format.nChannels * mMixFormat.Format.wBitsPerSample / 8;
    mMixFormat.Format.nAvgBytesPerSec = mMixFormat.Format.nSamplesPerSec * mMixFormat.Format.wBitsPerSample * mMixFormat.Format.nChannels / 8;

    return true;
    }
  //}}}
  //{{{
  bool setNumBufferSamples (int numBufferSamples) {
    mNumBufferSamples = numBufferSamples;
    return true;
    }
  //}}}

  //{{{
  bool start() {

    if (mAudioClient == nullptr) {
      //{{{
      cLog::log (LOGERROR, "audioClient failed");
      return false;
      }
      //}}}

    REFERENCE_TIME hnsRequestedDuration = 0;
    if (FAILED (mAudioClient->GetDevicePeriod (NULL, &hnsRequestedDuration))) {
      //{{{
      cLog::log (LOGERROR, "AudioClient->GetDevicePeriod failed");
      return false;
      }
      //}}}

    if (FAILED (mAudioClient->Initialize (AUDCLNT_SHAREMODE_SHARED,
                                          AUDCLNT_STREAMFLAGS_RATEADJUST | AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                          hnsRequestedDuration, hnsRequestedDuration,
                                          &mMixFormat.Format, nullptr))) {
      //{{{
      cLog::log (LOGERROR, "AudioClient->Initialize failed");
      return false;
      }
      //}}}

    mAudioClient->GetService (cWaspiUtil::getIAudioRenderClientInterfaceId(), reinterpret_cast<void**>(&mAudioRenderClient));
    mAudioClient->GetService (cWaspiUtil::getIAudioCaptureClientInterfaceId(), reinterpret_cast<void**>(&mAudioCaptureClient));

    if (FAILED (mAudioClient->GetBufferSize (&mNumBufferSamples))) {
      //{{{
      cLog::log (LOGERROR, "AudioClient->GetBufferSize failed");
      return false;
      }
      //}}}

    mEventHandle = CreateEvent (nullptr, FALSE, FALSE, nullptr);
    if (mEventHandle == nullptr) {
      //{{{
      cLog::log (LOGERROR, "start - createEvent failed");
      return false;
      }
      //}}}
    if (FAILED (mAudioClient->SetEventHandle (mEventHandle))) {
      //{{{
      cLog::log (LOGERROR, "AudioClient->SetEventHandle failed");
      return false;
      }
      //}}}

    if (FAILED (mAudioClient->Start()))  {
      //{{{
      cLog::log (LOGERROR, "AudioClient->Start failed");
      return false;
      }
      //}}}

    mSrcSamples = nullptr;
    mSrcSamplesLeft = 0;

    return true;
    }
  //}}}
  //{{{
  void process (const std::function<void (float*&, int&)>& callback) {

    if (isOutput()) {
      UINT32 currentPadding = 0;
      mAudioClient->GetCurrentPadding (&currentPadding);
      int numSamplesAvailable = mNumBufferSamples - currentPadding;
      if (numSamplesAvailable) {
        BYTE* data = nullptr;
        mAudioRenderClient->GetBuffer (numSamplesAvailable, &data);
        if (data) {
          auto dstSamples = reinterpret_cast<float*>(data);
          auto dstSamplesLeft = numSamplesAvailable;
          cLog::log (LOGINFO, " process %d", dstSamplesLeft);

          while (dstSamplesLeft > 0) {
            // loop till no dst filled
            if (mSrcSamplesLeft <= 0) // load more src
              callback (mSrcSamples, mSrcSamplesLeft);

            auto samplesChunk = std::min (mSrcSamplesLeft, dstSamplesLeft);
            if (mSrcSamples)
              memcpy (dstSamples, mSrcSamples, samplesChunk * mMixFormat.Format.nChannels * sizeof(float));
            else // no more src, silence
              memset (dstSamples, 0, samplesChunk * mMixFormat.Format.nChannels * sizeof(float));

            dstSamples += samplesChunk * mMixFormat.Format.nChannels;
            dstSamplesLeft -= samplesChunk;
            if (mSrcSamples)
              mSrcSamples += samplesChunk * mMixFormat.Format.nChannels;
            mSrcSamplesLeft -= samplesChunk;
            }

          mAudioRenderClient->ReleaseBuffer (numSamplesAvailable, 0);
          }
        }
      }
    }
  //}}}
  //{{{
  void process (const std::function<void (cAudioDevice&, cAudioBuffer&)>& callback) {

    if (isOutput()) {
      UINT32 currentPadding = 0;
      mAudioClient->GetCurrentPadding (&currentPadding);
      int numSamplesAvailable = mNumBufferSamples - currentPadding;
      if (numSamplesAvailable) {
        BYTE* data = nullptr;
        mAudioRenderClient->GetBuffer (numSamplesAvailable, &data);
        if (data) {
          cAudioBuffer audioBuffer = { reinterpret_cast<float*>(data), numSamplesAvailable, mMixFormat.Format.nChannels };
          callback (*this, audioBuffer);
          mAudioRenderClient->ReleaseBuffer (numSamplesAvailable, 0);
          }
        }
      }

    else if (isInput()) {
      UINT32 nextPacketSize = 0;
      mAudioCaptureClient->GetNextPacketSize (&nextPacketSize);
      if (nextPacketSize) {
        DWORD flags = 0;
        BYTE* data = nullptr;
        mAudioCaptureClient->GetBuffer (&data, &nextPacketSize, &flags, nullptr, nullptr);
        if (data) {
          cAudioBuffer audioBuffer = { reinterpret_cast<float*>(data), (int)nextPacketSize, mMixFormat.Format.nChannels };
          callback (*this, audioBuffer);
          mAudioCaptureClient->ReleaseBuffer (nextPacketSize);
          }
        }
      }
    }
  //}}}

  void wait() const { WaitForSingleObject (mEventHandle, INFINITE); }
  //{{{
  bool stop() {

    if (mAudioClient != nullptr)
      mAudioClient->Stop();
    if (mEventHandle != nullptr)
      CloseHandle (mEventHandle);

    return true;
    }
  //}}}

private:
  friend class cAudioDeviceEnumerator;
  //{{{
  cAudioDevice (IMMDevice* device, bool isRenderDevice)
      : mDevice(device), mIsRenderDevice(isRenderDevice) {

    // TODO: Handle errors better.  Maybe by throwing exceptions?
    if (mDevice == nullptr)
      throw sAudioDeviceException ("IMMDevice is null.");

    initDeviceIdName();
    if (mDeviceId.empty())
      throw sAudioDeviceException ("Could not get device id.");

    if (mName.empty())
      throw sAudioDeviceException ("Could not get device name.");

    initAudioClient();
    if (mAudioClient == nullptr)
      return;

    WAVEFORMATEX* deviceMixFormat;
    if (FAILED (mAudioClient->GetMixFormat (&deviceMixFormat))) {
      cLog::log (LOGERROR, "AudioClient->deviceMixFormat failed");
      return;
      }
    auto* deviceMixFormatEx = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(deviceMixFormat);
    mMixFormat = *deviceMixFormatEx;
    CoTaskMemFree (deviceMixFormat);
    }
  //}}}

  //{{{
  void initDeviceIdName() {

    LPWSTR deviceId = nullptr;
    HRESULT hr = mDevice->GetId (&deviceId);
    if (SUCCEEDED (hr)) {
      mDeviceId = deviceId;
      CoTaskMemFree (deviceId);
      }

    IPropertyStore* property_store = nullptr;
    cWaspiUtil::cAutoRelease auto_release_property_store { property_store };

    hr = mDevice->OpenPropertyStore (STGM_READ, &property_store);
    if (SUCCEEDED(hr)) {
      PROPVARIANT property_variant;
      PropVariantInit (&property_variant);

      auto try_acquire_name = [&](const auto& property_name) {
        hr = property_store->GetValue (property_name, &property_variant);
        if (SUCCEEDED(hr)) {
          mName = cWaspiUtil::convertString (property_variant.pwszVal);
          return true;
          }

        return false;
        };

      try_acquire_name (PKEY_Device_FriendlyName) ||
        try_acquire_name (PKEY_DeviceInterface_FriendlyName) ||
          try_acquire_name (PKEY_Device_DeviceDesc);

      PropVariantClear (&property_variant);
      }
    }
  //}}}
  //{{{
  void initAudioClient() {

    HRESULT hr = mDevice->Activate (cWaspiUtil::getIAudioClientInterfaceId(), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&mAudioClient));
    if (FAILED(hr))
      return;
    }
  //}}}

  IMMDevice* mDevice = nullptr;
  IAudioClient* mAudioClient = nullptr;
  IAudioCaptureClient* mAudioCaptureClient = nullptr;
  IAudioRenderClient* mAudioRenderClient = nullptr;
  HANDLE mEventHandle;

  std::string mName;
  std::wstring mDeviceId;

  bool mIsRenderDevice = true;
  WAVEFORMATEXTENSIBLE mMixFormat;
  UINT32 mNumBufferSamples = 0;

  float* mSrcSamples = nullptr;
  int mSrcSamplesLeft = 0;

  std::function <void (cAudioDevice&)> mStopCallback;
  std::variant <std::function<void (cAudioDevice&, cAudioBuffer&)>> mUserCallback;

  cWaspiUtil::cComInitializer mComInitializer;
  };
//}}}

// cAudioDeviceList
enum class cAudioDeviceListEvent { eListChanged, eDefaultInputChanged, eDefaultOutputChanged, };
class cAudioDeviceList : public std::forward_list <cAudioDevice> {};

// cAudioDeviceMonitor
//{{{
class cAudioDeviceMonitor {
public:
  //{{{
  static cAudioDeviceMonitor& instance() {

    static cAudioDeviceMonitor singleton;
    return singleton;
    }
  //}}}

  //{{{
  template <typename F> void registerCallback (cAudioDeviceListEvent event, F&& callback) {

    mCallbackMonitors[static_cast<int>(event)].reset (new WASAPINotificationClient { mEnumerator, event, std::move(callback)});
    }
  //}}}
  //{{{
  template <> void registerCallback (cAudioDeviceListEvent event, nullptr_t&&) {

    mCallbackMonitors[static_cast<int>(event)].reset();
    }
  //}}}

private:
  //{{{
  cAudioDeviceMonitor() {

    HRESULT hr = CoCreateInstance (__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_INPROC_SERVER,
                                   __uuidof(IMMDeviceEnumerator), (void**)&mEnumerator);
    if (FAILED(hr))
      throw sAudioDeviceException ("Could not create device enumerator");
    }
  //}}}
  //{{{
  ~cAudioDeviceMonitor() {

    if (mEnumerator == nullptr)
      return;

    for (auto& callback_monitor : mCallbackMonitors)
      callback_monitor.reset();

    mEnumerator->Release();
    }
  //}}}

  //{{{
  class WASAPINotificationClient : public IMMNotificationClient {
  public:
    //{{{
    WASAPINotificationClient (IMMDeviceEnumerator* enumerator, cAudioDeviceListEvent event, std::function<void()> callback) :
        mEnumerator(enumerator), mEvent(event), mCallback(std::move(callback)) {

      if (mEnumerator == nullptr)
        throw sAudioDeviceException("Attempting to create a notification client for a null enumerator");

      mEnumerator->RegisterEndpointNotificationCallback(this);
      }
    //}}}
    //{{{
    virtual ~WASAPINotificationClient() {

      mEnumerator->UnregisterEndpointNotificationCallback (this);
      }
    //}}}

    //{{{
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged (EDataFlow flow, ERole role, [[maybe_unused]] LPCWSTR deviceId) {

      if (role != ERole::eConsole)
        return S_OK;

      if (flow == EDataFlow::eRender) {
        if (mEvent != cAudioDeviceListEvent::eDefaultOutputChanged)
          return S_OK;
        }
      else if (flow == EDataFlow::eCapture) {
        if (mEvent != cAudioDeviceListEvent::eDefaultInputChanged)
          return S_OK;
        }

      mCallback();
      return S_OK;
      }
    //}}}

    //{{{
    HRESULT STDMETHODCALLTYPE OnDeviceAdded ([[maybe_unused]] LPCWSTR deviceId) {

      if (mEvent != cAudioDeviceListEvent::eListChanged)
        return S_OK;

      mCallback();
      return S_OK;
      }
    //}}}
    //{{{
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved ([[maybe_unused]] LPCWSTR deviceId) {

      if (mEvent != cAudioDeviceListEvent::eListChanged)
        return S_OK;

      mCallback();
      return S_OK;
      }
    //}}}
    //{{{
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged ([[maybe_unused]] LPCWSTR deviceId, [[maybe_unused]] DWORD new_state) {

      if (mEvent != cAudioDeviceListEvent::eListChanged)
        return S_OK;

      mCallback();

      return S_OK;
      }
    //}}}
    //{{{
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged ([[maybe_unused]] LPCWSTR deviceId, [[maybe_unused]] const PROPERTYKEY key) {

      return S_OK;
      }
    //}}}

    //{{{
    HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, VOID **requested_interface) {

      if (IID_IUnknown == riid)
        *requested_interface = (IUnknown*)this;
      else if (__uuidof(IMMNotificationClient) == riid)
        *requested_interface = (IMMNotificationClient*)this;
      else {
        *requested_interface = nullptr;
        return E_NOINTERFACE;
        }

      return S_OK;
      }
    //}}}
    ULONG STDMETHODCALLTYPE AddRef() { return 1; }
    ULONG STDMETHODCALLTYPE Release() { return 0; }

  private:
    cWaspiUtil::cComInitializer mComInitializer;
    IMMDeviceEnumerator* mEnumerator;
    cAudioDeviceListEvent mEvent;
    std::function<void()> mCallback;
    };
  //}}}

  cWaspiUtil::cComInitializer mComInitializer;

  IMMDeviceEnumerator* mEnumerator = nullptr;
  std::array <std::unique_ptr <WASAPINotificationClient>, 3> mCallbackMonitors;
  };
//}}}
//{{{
void setAudioDeviceListCallback (cAudioDeviceListEvent event, std::function<void()>&& callback) {
  cAudioDeviceMonitor::instance().registerCallback (event, std::move (callback));
  }
//}}}

// cAudioDeviceEnumerator
//{{{
class cAudioDeviceEnumerator {
public:
  static std::optional<cAudioDevice> getDefaultInputDevice() { return getDefaultDevice (false); }
  static std::optional<cAudioDevice> getDefaultOutputDevice() { return getDefaultDevice (true); }

  static auto getInputDeviceList() { return getDeviceList (false); }
  static auto getOutputDeviceList() { return getDeviceList (true); }

private:
  cAudioDeviceEnumerator() = delete;

  //{{{
  static std::optional<cAudioDevice> getDefaultDevice (bool outputDevice) {

    cWaspiUtil::cComInitializer comInitializer;

    IMMDeviceEnumerator* enumerator = nullptr;
    cWaspiUtil::cAutoRelease enumerator_release { enumerator };

    HRESULT hr = CoCreateInstance (cWaspiUtil::getMMDeviceEnumeratorClassId(), nullptr,
                                   CLSCTX_ALL, cWaspiUtil::getIMMDeviceEnumeratorInterfaceId(),
                                   reinterpret_cast<void**>(&enumerator));
    if (FAILED (hr))
      return std::nullopt;

    IMMDevice* device = nullptr;
    hr = enumerator->GetDefaultAudioEndpoint (outputDevice ? eRender : eCapture, eConsole, &device);
    if (FAILED(hr))
      return std::nullopt;

    try {
      return cAudioDevice { device, outputDevice };
      }
    catch (const sAudioDeviceException&) {
      return std::nullopt;
      }
    }
  //}}}
  //{{{
  static std::vector<IMMDevice*> getDevices (bool outputDevices) {

    cWaspiUtil::cComInitializer comInitializer;

    IMMDeviceEnumerator* enumerator = nullptr;
    cWaspiUtil::cAutoRelease enumerator_release { enumerator };
    HRESULT hr = CoCreateInstance (cWaspiUtil::getMMDeviceEnumeratorClassId(), nullptr,
                                   CLSCTX_ALL, cWaspiUtil::getIMMDeviceEnumeratorInterfaceId(),
                                   reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr))
      return {};

    IMMDeviceCollection* device_collection = nullptr;
    cWaspiUtil::cAutoRelease collection_release { device_collection };

    EDataFlow selected_data_flow = outputDevices ? eRender : eCapture;
    hr = enumerator->EnumAudioEndpoints (selected_data_flow, DEVICE_STATE_ACTIVE, &device_collection);
    if (FAILED (hr))
      return {};

    UINT device_count = 0;
    hr = device_collection->GetCount (&device_count);
    if (FAILED (hr))
      return {};

    std::vector<IMMDevice*> devices;
    for (UINT i = 0; i < device_count; i++) {
      IMMDevice* device = nullptr;
      hr = device_collection->Item (i, &device);
      if (FAILED(hr)) {
        if (device != nullptr)
          device->Release();
        continue;
        }

      if (device != nullptr)
        devices.push_back (device);
      }

    return devices;
    }
  //}}}
  //{{{
  static cAudioDeviceList getDeviceList (bool outputDevices) {

    cWaspiUtil::cComInitializer comInitializer;

    cAudioDeviceList devices;
    const auto mmdevices = getDevices (outputDevices);
    for (auto* mmdevice : mmdevices) {
      if (mmdevice == nullptr)
        continue;

      try {
        devices.push_front (cAudioDevice { mmdevice, outputDevices });
        }
      catch (const sAudioDeviceException&) {
        // TODO: Should I do anything with this exception?
        // My impulse is to leave it alone.  The result of this function
        // should be an array of properly-constructed devices.  If we
        // couldn't create a device, then we shouldn't return it from
        // this function.
        }
      }

    return devices;
    }
  //}}}
  };
//}}}
cAudioDeviceList getAudioInputDeviceList() { return cAudioDeviceEnumerator::getInputDeviceList(); }
cAudioDeviceList getAudioOutputDeviceList() { return cAudioDeviceEnumerator::getOutputDeviceList(); }
std::optional<cAudioDevice> getDefaultAudioInputDevice() { return cAudioDeviceEnumerator::getDefaultInputDevice(); }
std::optional<cAudioDevice> getDefaultAudioOutputDevice() { return cAudioDeviceEnumerator::getDefaultOutputDevice(); }
