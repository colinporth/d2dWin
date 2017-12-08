(*
Delphi 7 unit for the libfaad2 Wrapper Filter (LWF)
Copyright 2007-2008 Griga (http://www.dvbviewer.com)
Contact me via http://www.dvbviewer.info/forum/

This file is part of the LWF 1.1.3 source code.

    The LWF is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    The LWF is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the LWF. If not, see <http://www.gnu.org/licenses/>.
*)

(*
Extended by New Age

Some code parts/hints are from libfaac-1.28
*)

unit UParser;
//contains the LOAS/LATM/ADTS parser classes

interface

uses
  Windows, SysUtils;

type
  TBitStream = class
  //for reading and copying LATM data bitwise
  protected
    Data: PByte;
    nMax: Integer;
    CurByte: Byte;
    BitPos: Byte;
  public
    CopyStream: TBitStream;
    nBits: Integer;
    constructor CreateInit(Buf: PByte; Size: Integer);
    function GetByte: Byte;
    procedure SetByte(b: Byte);
    function GetBit: Byte;
    procedure SetBit(b: Byte);
    procedure SetBits(bits: Cardinal; count_of_lowerbits: Byte);
    function GetVal(n: Integer): DWord;
    procedure SetByteAlign(b: Byte);
    function LATMGetValue: DWord;
    procedure Init(Buf: PByte; Size: Integer);
  end;

  TAudioSpecificConfig = record
    AudioObjectType: Word;
	  SamplingFrequencyIndex: Byte;
	  SamplingFrequency: DWord;
	  ChannelConfiguration: Byte;
    //for explicit SBR signaling (AudioObjectType = 5)
    sbrPresentFlag: Byte;
    extensionAudioObjectType: Word;
    extensionSamplingFrequencyIndex: Byte;
    extensionSamplingFrequency: DWord;
    //for storing the audio specific config as passed to faad2
    Extra: array[0..63] of Byte; // should be way enough
 	  ExtraSize: Integer;
  end;
  PAudioSpecificConfig = ^TAudioSpecificConfig;

  TLatmStream = record //per stream data
    progSIndex: Integer;
    laySIndex: Integer;
    frameLengthType: Byte;
    latmBufferFullness: Byte;
    frameLength: Word;
    muxSlotLengthBytes: Word;
    muxSlotLengthCoded: Byte;
    AuEndFlag: Byte;
  end;

  TAACParser = class
  protected
    // internals
    bsr: TBitStream; //read only
    streamID: array[0..15,0..7] of DWord;

    //per program data
    numLayer: array[0..15] of Byte;

    //per stream data
    streams: array of TLatmStream;

    //per chunk data
    progCIndx: array[0..15] of Byte;
    layCIndx: array[0..15] of Byte;

    audio_mux_version: Byte;
    audio_mux_version_A: Byte;
    all_same_framing: Byte;
    taraFullness: DWord;
    config_crc: Byte;
    other_data_bits: DWord;
    numProgram: DWord;
    numSubFrames: DWord;
    streamCnt: DWord;
    numChunk: DWord;

    function StreamMuxConfig: Boolean;
    procedure ProgramConfigElement;
    procedure GASpecificConfig(var cfg: TAudioSpecificConfig);
    function PayloadLengthInfo: Boolean;
    function ReadLOASHeader(Buf: PByte; Size: Integer; var muxLength: Integer): Integer;
    function ReadADTSHeader(Buf: PByte; Size: Integer; var FrameSize: Integer): Integer;
    function WriteADTSHeader(OutBuf: PByte; MaxOutSize: Integer; Payload: PByte; PayloadSize: Word): Integer;
  public
    config: array of TAudioSpecificConfig;
    constructor Create;
    destructor Destroy; override;
    function ReadAudioSpecificConfig(var cfg: TAudioSpecificConfig): Integer;
    function ReadLATMData(Buf: PByte; Size: Integer): Boolean;
    function ReadLATMPacket(Buf: PByte; Size: Integer; Payload: PByte; var payloadsize: Integer): Integer;
    function ReadADTSData(Buf: PByte; Size: Integer): Boolean;
    function ReadADTSPacket(Buf: PByte; Size: Integer; Payload: PByte; var payloadsize: Integer): Integer;
    function WriteADTSPacket(OutBuf: PByte; MaxOutSize: Integer; Payload: PByte; PayloadSize: Integer): Integer;
  end;

  TConfigData = record
    AudioObjectType: Word;
    Samplerate: DWord;
    Channels: Byte;
  end;

  TMPEG4AudioObjectType = ({  0 }aotNull,
                           {  1 }aotMain,
                           {  2 }aotLowComplexity,
                           {  3 }aotScalableSampleRate,
                           {  4 }aotLongTermPrediction,
                           {  5 }aotSpectralBandReplication,
                           {  6 }aotAACScalable,
                           {  7 }aotTwinVQ,
                           {  8 }aotCodeExcitedLinearPrediction,
                           {  9 }aotHarmonicVectoreXcitationCoding,
                           { 10 }aotReserved10,
                           { 11 }aotReserved11,
                           { 12 }aotTextToSpeechInterface,
                           { 13 }aotMainSynthesis,
                           { 14 }aotWavetableSynthesis,
                           { 15 }aotGeneralMIDI,
                           { 16 }aotAlgorithmicSynthesisAndAudioEffects,
                           { 17 }aotErrorResilientLowComplexity,
                           { 18 }aotReserved18,
                           { 19 }aotErrorResilientLongTermPrediction,
                           { 20 }aotErrorResilientScalable,
                           { 21 }aotErrorResilientTwinVQ,
                           { 22 }aotErrorResilientBitSlicedArithmeticCoding,
                           { 23 }aotErrorResilientLowDelay,
                           { 24 }aotErrorResilientCodeExcitedLinearPrediction,
                           { 25 }aotErrorResilientHarmonicVectoreXcitationCoding,
                           { 26 }aotErrorResilientHarmonicAndIndividualLinesPlusNoise,
                           { 27 }aotErrorResilientParametric,
                           { 28 }aotSinuSoidalCoding,
                           { 29 }aotParametricStereo,
                           { 30 }aotMPEGSurround,
                           { 31 }aotEscapeValue,
                           { 32 }aotLayer1,
                           { 33 }aotLayer2,
                           { 34 }aotLayer3,
                           { 35 }aotDirectStreamTransfer,
                           { 36 }aotAudioLossless,
                           { 37 }aotScalableLosslesS,
                           { 38 }aotScalableLosslesSNonCore,
                           { 39 }aotErrorResilientEnhancedLowDelay,
                           { 40 }aotSymbolicMusicRepresentationSimple,
                           { 41 }aotSymbolicMusicRepresentationMain );
  TADTSObjectType = (otMAIN, otLOW, otSSR, otLTP);      // 0, 1, 2, 3 (2 bits)


procedure ReadConfig(Config: PByteArray; var ConfigData: TConfigData);
function WriteConfig(Config: PByteArray; const ConfigData: TConfigData): Integer;
function ConfigEqual(Config1, Config2: PWord): Boolean;

implementation

const
  AAC_SAMPLE_RATES: array[0..15] of DWord =
    (96000,88200,64000,48000,44100,32000,24000, 22050,
     16000,12000,11025, 8000, 7350, 0,  0,  0);

  ADTS_HEADER_SIZE = 7;

  TMPEG4AudioObjectTypeToTADTSObjectType: array [TMPEG4AudioObjectType] of TADTSObjectType =
                                        ({  0 }otMAIN,
                                         {  1 }otMAIN,
                                         {  2 }otLOW,
                                         {  3 }otSSR,
                                         {  4 }otLTP,
                                         {  5 }otMAIN,
                                         {  6 }otSSR,
                                         {  7 }otMAIN,
                                         {  8 }otMAIN,
                                         {  9 }otMAIN,
                                         { 10 }otMAIN,
                                         { 11 }otMAIN,
                                         { 12 }otMAIN,
                                         { 13 }otMAIN,
                                         { 14 }otMAIN,
                                         { 15 }otMAIN,
                                         { 16 }otMAIN,
                                         { 17 }otLOW,
                                         { 18 }otMAIN,
                                         { 19 }otLTP,
                                         { 20 }otSSR,
                                         { 21 }otMAIN,
                                         { 22 }otMAIN,
                                         { 23 }otMAIN,
                                         { 24 }otMAIN,
                                         { 25 }otMAIN,
                                         { 26 }otMAIN,
                                         { 27 }otMAIN,
                                         { 28 }otMAIN,
                                         { 29 }otMAIN,
                                         { 30 }otMAIN,
                                         { 31 }otMAIN,
                                         { 32 }otMAIN,
                                         { 33 }otMAIN,
                                         { 34 }otMAIN,
                                         { 35 }otMAIN,
                                         { 36 }otMAIN,
                                         { 37 }otMAIN,
                                         { 38 }otMAIN,
                                         { 39 }otMAIN,
                                         { 40 }otMAIN,
                                         { 41 }otMAIN );

//The audio_specific_config contains at least a 2 bytes bitfield:
//5 bits: audio_object_type resp. layer from ADTS header
//4 bits: sampling_frequency_index
//4 bits: channel_configuration (mostly = number of channels)
//3 bits: = 0

procedure ReadConfig(Config: PByteArray; var ConfigData: TConfigData);
//Reads the samplerate and number of channels from the audio_specific_config
begin
  with ConfigData do
  begin
    AudioObjectType := Config[0] shr 3;
    Samplerate := AAC_SAMPLE_RATES[((Config[0] and $07) shl 1) or (Config[1] shr 7)];
    Channels := (Config[1] and $78) shr 3;
  end;
end;

function WriteConfig(Config: PByteArray; const ConfigData: TConfigData): Integer;
//Creates the audio_specific_config bitfield
var
  i: Integer;
  Index: Byte;
begin
  with ConfigData do
  begin
    Index := 3; //default 48 khz
    for i := 0 to 12 do
      if Samplerate = AAC_SAMPLE_RATES[i] then
      begin
        Index := i;
        break;
      end;
    Config[0] := (AudioObjectType shl 3) or (Index shr 1);
    Config[1] := (Index shl 7) or (Channels shl 3);
  end;
  result := 2;
end;

function ConfigEqual(Config1, Config2: PWord): Boolean;
//returns true if Config1 and Config2 are equal
begin
  result := (Config1^ xor Config2^) and $FFF8 = 0;
end;

//----------------------------------------------------- TBitStream Class

function TBitStream.GetByte: Byte;
begin
  Dec(nMax);
  if nMax < 0 then
    Exception.Create('');
  result := Data^;
  Inc(Data);
end;

procedure TBitStream.SetByte(b: Byte);
begin
  Dec(nMax);
  if nMax < 0 then
    Exception.Create('');
  Data^ := b;
  Inc(Data);
end;

function TBitStream.GetBit: Byte;
begin
  if BitPos = $80 then
    CurByte := GetByte;
  result := Ord(CurByte and BitPos <> 0);
  Inc(nBits);
  if assigned(CopyStream) then
    CopyStream.SetBit(result);
  BitPos := BitPos shr 1;
  if BitPos = 0 then
    BitPos := $80;
end;

procedure TBitStream.SetBit(b: Byte);
begin
  if b <> 0 then
    Inc(CurByte,BitPos);
  BitPos := BitPos shr 1;
  Inc(nBits);
  if BitPos = 0 then
  begin
    SetByte(CurByte);
    CurByte := 0;
    BitPos := $80;
  end;
end;

procedure TBitStream.SetBits(bits: Cardinal; count_of_lowerbits: Byte);
begin
  if count_of_lowerbits > 0 then
  begin
    bits := (bits shl (32 - count_of_lowerbits)) and $FFFFFFFF;
    while count_of_lowerbits > 0 do
    begin
      SetBit(Ord((bits and $80000000) <> 0));
      bits := (bits shl 1) and $FFFFFFFF;
      Dec(count_of_lowerbits);
    end;
  end;
end;

procedure TBitStream.SetByteAlign(b: Byte);
begin
  while BitPos <> $80 do
    SetBit(b);
end;

function TBitStream.GetVal(n: Integer): DWord;
var
  i: Integer;
begin
  result := 0;
  for i := 0 to n-1 do
    result := result shl 1 + GetBit;
end;

function TBitStream.LATMGetValue: DWord;
begin
  result := GetVal((GetVal(2)+1)*8);
  {
  result := 0;
  for i := 0 to GetVal(2) do
    result := result shl 8 + GetVal(8);
  }
end;

procedure TBitStream.Init(Buf: PByte; Size: Integer);
begin
  Data := Buf;
  nMax := Size;
  BitPos := $80;
  CurByte := 0;
  nBits := 0;
end;

constructor TBitStream.CreateInit(Buf: PByte; Size: Integer);
begin
  inherited Create;
  Init(Buf,Size);
end;

//----------------------------------------------------- TAACParser class

constructor TAACParser.Create;
begin
  inherited;
  bsr := TBitStream.Create;
end;

destructor TAACParser.Destroy;
begin
  bsr.Free;
  inherited;
end;

//----------------------- LATM stuff

procedure TAACParser.ProgramConfigElement;
//we just read the data here, thus coying it to config[streamCnt].Extra,
//but we don't store the values separately.
var
  num_front_channel_elements, num_side_channel_elements,
  num_back_channel_elements, num_lfe_channel_elements,
  num_assoc_data_elements, num_valid_cc_elements,
  comment_field_bytes, i: Integer;
begin
  with bsr do
  begin
    GetVal(4); //element_instance_tag;
    GetVal(2); //object_type;
    GetVal(4); //sampling_frequency_index;
    num_front_channel_elements := GetVal(4);
    num_side_channel_elements := GetVal(4);
    num_back_channel_elements := GetVal(4);
    num_lfe_channel_elements := GetVal(2);
    num_assoc_data_elements := GetVal(3);
    num_valid_cc_elements := GetVal(4);
    if GetBit <> 0 then //mono_mixdown_present
      GetVal(4); //mono_mixdown_element_number
    if GetBit <> 0 then //stereo_mixdown_present
      GetVal(4); //stereo_mixdown_element_number
    if GetBit <> 0 then // matrix_mixdown_idx_present
    begin
      GetVal(2); //matrix_mixdown_idx
      GetBit; //pseudo_surround_enable;
    end;
    for i := 0 to num_front_channel_elements-1 do
    begin
      GetBit; //front_element_is_cpe[i]
      GetVal(4); //front_element_tag_select[i]
    end;
    for i := 0 to num_side_channel_elements-1 do
    begin
      GetBit; //side_element_is_cpe[i]
      GetVal(4); //side_element_tag_select[i]
    end;
    for i := 0 to num_back_channel_elements-1 do
    begin
      GetBit; //back_element_is_cpe[i];
      GetVal(4); //back_element_tag_select[i];
    end;
    for i := 0 to num_lfe_channel_elements-1 do
      GetVal(4); //lfe_element_tag_select[i];
    for i := 0 to num_assoc_data_elements-1 do
      GetVal(4); //assoc_data_element_tag_select[i];
    for i := 0 to num_valid_cc_elements-1 do
    begin
      GetBit; //cc_element_is_ind_sw[i]
      GetVal(4); //valid_cc_element_tag_select[i];
    end;
    i := bsr.CopyStream.nBits mod 8; //number of bits in audio_specific_config
    if i <> 0 then
      GetVal(8-i); //byte_alignment() relative to audio specific config
    comment_field_bytes := GetVal(8);
    for i := 0 to comment_field_bytes-1 do
      GetVal(8); //comment_field_data[i]
  end;
end;

procedure TAACParser.GASpecificConfig(var cfg: TAudioSpecificConfig);
//we just read the data here, thus coying it to config[streamCnt].Extra,
//but we don't store the values separately.
var
  dependsOnCoder, ext_flag {, framelen_flag}: Byte;
  //delay, layerNr, numOfSubFrame, layer_length: DWord;
begin
  with cfg, bsr do
  begin
    {framelen_flag :=} GetBit;
    dependsOnCoder := GetBit;
    if dependsOnCoder <> 0 then
      {delay :=} GetVal(14);
    ext_flag := GetBit;
    if channelConfiguration = 0 then
      ProgramConfigElement;
    if (audioObjectType = 6) or (audioObjectType = 20) then
      {layerNr :=} GetVal(3);
    if ext_flag <> 0 then
    begin
      case audioObjectType of
        22:
          begin
            {numOfSubFrame :=} GetVal(5);
            {layer_length :=} GetVal(11);
          end;
        17,19,20,23:
          begin
            GetVal(3); //stuff
          end;
      end;
      GetBit; //extflag3
    end;
  end;
end;

function TAACParser.ReadAudioSpecificConfig(var cfg: TAudioSpecificConfig): Integer;
begin
  with cfg, bsr do
  try
    CopyStream := TBitStream.CreateInit(@Extra,SizeOf(Extra)); //write only
    audioObjectType := GetVal(5);
    if audioObjectType = 31 then //audioObjectTypeExt
      audioObjectType := 32 + GetVal(6);
    samplingFrequencyIndex := GetVal(4);
    if samplingFrequencyIndex = 15 then
      samplingFrequency := GetVal(24)
    else
      samplingFrequency := AAC_SAMPLE_RATES[samplingFrequencyIndex];
    channelConfiguration := GetVal(4);
    sbrPresentFlag := 0;
    extensionAudioObjectType := 0;
    if audioObjectType = 5 then //explicit SBR signaling, used by Brazilian DVB-T
    begin
      extensionAudioObjectType := audioObjectType;
      sbrPresentFlag := 1;
      extensionSamplingFrequencyIndex := GetVal(4);
      if extensionSamplingFrequencyIndex = 15 then
        extensionSamplingFrequency := GetVal(24)
      else
        extensionSamplingFrequency := AAC_SAMPLE_RATES[extensionSamplingFrequencyIndex];
      //------ this was missing for Brazil ;-)
      audioObjectType := GetVal(5);
      if audioObjectType = 31 then //audioObjectTypeExt
        audioObjectType := 32 + GetVal(6);
      //---------------------------------------
    end;
    case audioObjectType of
      1..4,6,7,17,19,22..23:
        GASpecificConfig(cfg);
    end;
    result := CopyStream.nBits;
    CopyStream.SetByteAlign(0);
    ExtraSize := CopyStream.nBits div 8;
  finally
    FreeAndNil(CopyStream);
  end;
end;

function TAACParser.StreamMuxConfig: Boolean;
//LATM parsing according to ISO/IEC 14496-3
var
  ascLen, num, prog, lay: Integer;
  frame_length_type: DWord;
  //celp_table_index, hvxc_table_index, core_frame_offset: DWord;
  use_same_config, esc: Byte;
  cfg: PAudioSpecificConfig;
  objTypes: array[0..7] of DWord;
begin
  result := false;
  streamCnt := 0;
  SetLength(streams,0);
  SetLength(config,0);

	audio_mux_version := bsr.GetBit;
	audio_mux_version_A := 0;
	if audio_mux_version = 1 then
		audio_mux_version_A := bsr.GetBit;
	if audio_mux_version_A <> 0 then
    exit; //TBD

  if audio_mux_version = 1 then
    taraFullness := bsr.LatmGetValue;
  all_same_framing := bsr.GetBit;
  numSubFrames := bsr.GetVal(6);
  numProgram := bsr.GetVal(4);

  //DVB only uses one subframe, one program and one layer (since multiplexing is already
  //handled on the TS layer), and allStreamsSameTimeFraming is always = 1. Though
  //the StreamMuxConfig is parsed and stored completely (maybe we can use it later), the
  //libfaad2 wrapper actually supports only playback of the first program and layer.

  for prog := 0 to numProgram do
  begin
    num := bsr.GetVal(3);
    numLayer[prog] := num;
    SetLength(streams,Length(streams)+num+1);
    SetLength(config,Length(config)+num+1);
    for lay := 0 to num do
    begin
      with streams[streamCnt] do
      begin
        cfg := @config[streamCnt];
        streamID[prog,lay] := streamCnt;
        progSIndex := prog;
        laySIndex := lay;
        if (prog = 0) and (lay = 0) then
          use_same_config := 0
        else
          use_same_config := bsr.GetBit;
        if use_same_config <> 0 then // same as before
          cfg^ := config[streamCnt-1]
        else if audio_mux_version = 0 then //audio specific config.
          ReadAudioSpecificConfig(cfg^)
        else begin
          ascLen := Integer(bsr.LatmGetValue) - ReadAudioSpecificConfig(cfg^);
          if ascLen > 0 then
            bsr.GetVal(ascLen);
        end;
        objTypes[lay] := cfg.audioObjectType;
        // these are not needed... perhaps
        frame_length_type := bsr.GetVal(3);
        frameLengthType := frame_length_type;
        case frame_length_type of
          0:
            begin
              latmBufferFullness := bsr.GetVal(8);
              if all_same_framing = 0 then
                if ((objTypes[lay] = 6) or (objTypes[lay] = 20)) and
                   ((objTypes[lay-1] = 8) or (objTypes[lay-1] = 24)) then
                  {core_frame_offset :=} bsr.GetVal(6);
            end;
          1:
            frameLength := bsr.GetVal(9);
          3,4,5:
            {celp_table_index :=} bsr.GetVal(6);
          6,7:
            {hvxc_table_index :=} bsr.GetVal(1);
        end;
      end; //with streams[streamCnt] do
      Inc(streamCnt);
    end; //for lay := 0 to numLayer[prog]
  end; //for prog := 0 to numProgram

  // other data
  other_data_bits := 0;
  if bsr.GetBit > 0 then // other data present
    if audio_mux_version = 1 then
      other_data_bits := bsr.LATMGetValue
    else
      repeat
        esc := bsr.GetBit;
        other_data_bits := (other_data_bits shl 8) or bsr.GetVal(8);
      until esc = 0;

  // CRC
  if bsr.GetBit > 0 then
    config_crc := bsr.GetVal(8);

  result := true;
end;

function TAACParser.ReadLOASHeader(Buf: PByte; Size: Integer; var muxLength: Integer): Integer;
var
  i: Integer;
  PB: PByteArray absolute Buf;
begin
  result := -1;
  dec(Size,3); //3 bytes minimum to find LOAS header
  for i := 0 to Size do
  begin
    //check LOAS AudioSyncStream SyncWord
    if (PB[0] = $56) and
       (PB[1] and $E0 = $E0) then
    begin
      muxLength := Swap(PWord(@PB[1])^) and $1FFF;
      if (i + muxLength + 3 > Size) or
         ((PB[muxLength+3] = $56) and (PB[muxLength+4] and $E0 = $E0)) then
      begin
        result := i;
        break;
      end;
    end;
    Inc(Buf);
  end;
end;

function TAACParser.ReadLATMData(Buf: PByte; Size: Integer): Boolean;
//used for reading the audio specific config from a LOAS/LATM packet
//appended to WaveFormatEx by the source filter / demuxer
//returns true if successful
var
  i, muxLength: Integer;
begin
  try
    result := false;
    if Buf = nil then
      exit;
    i := ReadLOASHeader(Buf,Size,muxLength); //i = position of next loas header
    if i >= 0 then //loas/latm
    begin
      Inc(Buf,i+3);  //skip loas header
      Dec(Size,i+3);
      if Size > 0 then
      begin
        bsr.Init(Buf,Size);
        if bsr.GetBit = 0 then //use_same_mux = 0
          result := StreamMuxConfig;
      end;
    end;
  except
    result := false;
  end;
end;

function TAACParser.ReadLATMPacket(Buf: PByte; Size: Integer; Payload: PByte; var payloadsize: Integer): Integer;
//used for extracting raw AAC data (the payload) from LOAS/LATM packets
//returns the number of bytes that are consumed
var
  i,j, muxLength: Integer;
begin
	// ISO/IEC 14496-3 Table 1.28 - Syntax of AudioMuxElement()
  payloadsize := 0;
  i := ReadLOASHeader(Buf,Size,muxLength); //i = position of next LOAS header;
  if i < 0 then //no loas header found
  begin
    //skip all but 2 bytes, so no LOAS header gets lost
    result := Size - 2;
    if result < 0 then
      result := 0;
    exit;
  end;
  Inc(Buf,i+3); //skip useless leading bytes and loas header
  Dec(Size,i+3);
  if muxLength > Size then //not enough data
  begin
    result := i; //skip everything up to (not including) the next LOAS header
    exit;
  end;

  bsr.Init(Buf,Size);
  result := muxLength + i + 3;
  try
    if (bsr.GetBit = 0) and
       not StreamMuxConfig then //use_same_mux = 0 and reading Config failed
      exit;
    if audio_mux_version_A <> 0 then  //TBD
      exit;
    for i := 0 to numSubFrames do
    begin
      PayloadLengthInfo;
      payloadsize := streams[0].muxSlotLengthBytes;
      for j := 0 to payloadsize do //read payload
      begin
        Payload^ := bsr.GetVal(8);
        Inc(Payload);
      end;
    end;
  except
  end;
end;

function TAACParser.PayloadLengthInfo: Boolean;
var
  streamIndex: Byte;
  prog, lay, chunkCnt, tmp: DWord;
begin
	if all_same_framing <> 0 then
  begin
    for prog := 0 to numProgram do
      for lay := 0 to numLayer[prog] do
        with streams[streamId[prog][lay]] do
          if frameLengthType = 0 then
          begin
            muxSlotLengthBytes := 0;
            repeat
              tmp := bsr.GetVal(8);
              Inc(muxSlotLengthBytes,tmp);
            until tmp <> 255;
          end
          else if frameLengthType in [3,5,7] then
            muxSlotLengthCoded := bsr.GetVal(2);
  end
  else begin
		numChunk := bsr.GetVal(4);
    for chunkCnt := 0 to numChunk do
    begin
			streamIndex := bsr.GetVal(4);
			prog := streams[streamIndex].progSIndex;
      progCIndx[chunkCnt] := prog;
			lay  := streams[streamIndex].laySIndex;
      layCIndx[chunkCnt] := lay;
      with streams[streamId[prog][lay]] do
        if frameLengthType = 0 then
        begin
          muxSlotLengthBytes := 0;
          repeat
            tmp := bsr.GetVal(8);
            Inc(muxSlotLengthBytes,tmp);
          until tmp <> 255;
          AuEndFlag := bsr.GetBit;
        end
        else if frameLengthType in [3,5,7] then
          muxSlotLengthCoded := bsr.GetVal(2);
    end;
  end;
  result := true;
end;

//----------------------- ADTS stuff

function TAACParser.ReadADTSHeader(Buf: PByte; Size: Integer; var FrameSize: Integer): Integer;
var
  i: Integer;
  PB: PByteArray absolute Buf;
begin
  result := -1;
  dec(Size,7); //7 bytes minimum to read ADTS header
  for i := 0 to Size do
  begin
    if (PB[0] = $FF) and (PB[1] and $F6 = $F0) then
    begin
      FrameSize := Integer(PB[3] and $03) shl 11 +
                   Integer(PB[4]) shl 3 +
                   Integer(PB[5] shr 5);
      if FrameSize > 7 then
        if (i + FrameSize > Size) or
           //check next header, if possible
           ((PB[FrameSize] = $FF) and (PB[FrameSize+1] and $F6 = $F0)) then
          with config[0] do
          begin
            SamplingFrequencyIndex := (PB[2] and $3C) shr 2;
            SamplingFrequency := AAC_SAMPLE_RATES[SamplingFrequencyIndex];
            ChannelConfiguration := (PB[2] and $01) shl 2 + PB[3] shr 6;
            AudioObjectType := PB[2] shr 6; //AAC profile
            if (SamplingFrequency > 0) and (ChannelConfiguration > 0) then
            begin
              //compile extra data:
              //5 bits: audio_object_type resp. layer from ADTS header
              //4 bits: sampling_frequency_index
              //4 bits: channel_configuration (mostly = number of channels)
              //3 bits: = 0
              ExtraSize := 2;
              Extra[0] := (AudioObjectType shl 3) or (SamplingFrequencyIndex shr 1);
              Extra[1] := (SamplingFrequencyIndex shl 7) or (ChannelConfiguration shl 3);
              result := i;
              break;
            end;
          end;
    end;
    Inc(Buf);
  end;
end;

function TAACParser.WriteADTSHeader(OutBuf: PByte; MaxOutSize: Integer;
  Payload: PByte; PayloadSize: Word): Integer;
// total 56 bits (7 bytes)
var
  aot: Cardinal;
begin
   bsr.Init(OutBuf, MaxOutSize);

   // /* Fixed ADTS header */
   // Byte 0, Byte 1
   bsr.SetBits($FFF, 12); // 12 bit Syncword
   bsr.SetBits(0, 1);     // ID == 0 for MPEG4 AAC, 1 for MPEG2 AAC               // FIXME: should be written from config
   bsr.SetBits(0, 2);     // layer = 0
   bsr.SetBits(1, 1);     // protection absent
   // Byte 2, Byte 3
   aot := Ord(TMPEG4AudioObjectTypeToTADTSObjectType[TMPEG4AudioObjectType(config[0].AudioObjectType)]);
   bsr.SetBits(aot, 2);                              // AAC profile
   bsr.SetBits(config[0].SamplingFrequencyIndex, 4); // sampling rate
   bsr.SetBits(0, 1);                                // private bit
   bsr.SetBits(config[0].ChannelConfiguration, 3);   // ch. config (must be > 0) simply using numChannels only works for 6 channels or less, else a channel configuration should be written
   bsr.SetBits(0, 1);                                // original copy
   bsr.SetBits(0, 1);                                // home

   // /* Variable ADTS header */
   // Byte 3 cont., Byte 4, Byte 5, Byte 6
   bsr.SetBits(0, 1);                               // copyr. id. bit
   bsr.SetBits(0, 1);                               // copyr. id. start
   bsr.SetBits(PayloadSize + ADTS_HEADER_SIZE, 13); // frame size (header + raw)
   bsr.SetBits($7FF, 11);                           // buffer fullness (0x7FF for VBR)
   bsr.SetBits(0, 2);                               // raw data blocks (0+1=1)

   Result := ADTS_HEADER_SIZE;
end;

function TAACParser.ReadADTSPacket(Buf: PByte; Size: Integer; Payload: PByte; var payloadsize: Integer): Integer;
var
  i,FrameSize: Integer;
begin
  SetLength(config,1);
  payloadsize := 0;
  i := ReadADTSHeader(Buf,Size,FrameSize); //i = position of next ADTS header;
  if i < 0 then //no ADTS header found
  begin
    //skip all but 7 bytes, so no ADTS header gets lost
    result := Size - ADTS_HEADER_SIZE;
    if result < 0 then
      result := 0;
    exit;
  end;
  Inc(Buf,i); //skip useless leading bytes
  Dec(Size,i);
  if FrameSize > Size then //not enough data
  begin
    result := i; //skip everything up to (not including) the next ADTS header
    exit;
  end;
  result := FrameSize + i;
  payloadsize := FrameSize - ADTS_HEADER_SIZE;
  Inc(Buf, ADTS_HEADER_SIZE);
  move(Buf^,Payload^,payloadsize);
end;

function TAACParser.WriteADTSPacket(OutBuf: PByte; MaxOutSize: Integer;
  Payload: PByte; PayloadSize: Integer): Integer;
var
  OPB: PByteArray absolute OutBuf;
begin
  Result := WriteADTSHeader(OutBuf, MaxOutSize, Payload, PayloadSize);
  if (MaxOutSize - Result) >= PayloadSize then
  begin
    Move(Payload^, OPB[Result], PayloadSize);
    Inc(Result, PayloadSize);
  end
  else
    Result := 0; // error not enought space to write header + raw data
end;

function TAACParser.ReadADTSData(Buf: PByte; Size: Integer): Boolean;
var
  FrameSize: Integer;
begin
  SetLength(config,1);
  result := ReadADTSHeader(Buf,Size,FrameSize) = 0;
end;

end.
