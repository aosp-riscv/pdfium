// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#ifndef XFA_FXFA_FXFA_BASIC_H_
#define XFA_FXFA_FXFA_BASIC_H_

#include "fxjs/fxjse.h"

class CXFA_Measurement;
enum class XFA_ObjectType;
struct XFA_SCRIPTATTRIBUTEINFO;

enum XFA_HashCode : uint32_t {
  XFA_HASHCODE_None = 0,

  XFA_HASHCODE_Config = 0x4e1e39b6,
  XFA_HASHCODE_ConnectionSet = 0xe14c801c,
  XFA_HASHCODE_Data = 0xbde9abda,
  XFA_HASHCODE_DataDescription = 0x2b5df51e,
  XFA_HASHCODE_Datasets = 0x99b95079,
  XFA_HASHCODE_DataWindow = 0x83a550d2,
  XFA_HASHCODE_Event = 0x185e41e2,
  XFA_HASHCODE_Form = 0xcd309ff4,
  XFA_HASHCODE_Group = 0xf7f75fcd,
  XFA_HASHCODE_Host = 0xdb075bde,
  XFA_HASHCODE_Layout = 0x7e7e845e,
  XFA_HASHCODE_LocaleSet = 0x5473b6dc,
  XFA_HASHCODE_Log = 0x0b1b3d22,
  XFA_HASHCODE_Name = 0x31b19c1,
  XFA_HASHCODE_Occur = 0xf7eebe1c,
  XFA_HASHCODE_Pdf = 0xb843dba,
  XFA_HASHCODE_Record = 0x5779d65f,
  XFA_HASHCODE_Signature = 0x8b036f32,
  XFA_HASHCODE_SourceSet = 0x811929d,
  XFA_HASHCODE_Stylesheet = 0x6038580a,
  XFA_HASHCODE_Template = 0x803550fc,
  XFA_HASHCODE_This = 0x2d574d58,
  XFA_HASHCODE_Xdc = 0xc56afbf,
  XFA_HASHCODE_XDP = 0xc56afcc,
  XFA_HASHCODE_Xfa = 0xc56b9ff,
  XFA_HASHCODE_Xfdf = 0x48d004a8,
  XFA_HASHCODE_Xmpmeta = 0x132a8fbc
};

enum class XFA_PacketType : uint8_t {
  User,
  SourceSet,
  Pdf,
  Xdc,
  Xdp,
  Xmpmeta,
  Xfdf,
  Config,
  LocaleSet,
  Stylesheet,
  Template,
  Signature,
  Datasets,
  Form,
  ConnectionSet,
};

enum XFA_XDPPACKET {
  XFA_XDPPACKET_UNKNOWN = 0,
  XFA_XDPPACKET_Config = 1 << static_cast<uint8_t>(XFA_PacketType::Config),
  XFA_XDPPACKET_Template = 1 << static_cast<uint8_t>(XFA_PacketType::Template),
  XFA_XDPPACKET_Datasets = 1 << static_cast<uint8_t>(XFA_PacketType::Datasets),
  XFA_XDPPACKET_Form = 1 << static_cast<uint8_t>(XFA_PacketType::Form),
  XFA_XDPPACKET_LocaleSet = 1
                            << static_cast<uint8_t>(XFA_PacketType::LocaleSet),
  XFA_XDPPACKET_ConnectionSet =
      1 << static_cast<uint8_t>(XFA_PacketType::ConnectionSet),
  XFA_XDPPACKET_SourceSet = 1
                            << static_cast<uint8_t>(XFA_PacketType::SourceSet),
  XFA_XDPPACKET_Xdc = 1 << static_cast<uint8_t>(XFA_PacketType::Xdc),
  XFA_XDPPACKET_Pdf = 1 << static_cast<uint8_t>(XFA_PacketType::Pdf),
  XFA_XDPPACKET_Xfdf = 1 << static_cast<uint8_t>(XFA_PacketType::Xfdf),
  XFA_XDPPACKET_Xmpmeta = 1 << static_cast<uint8_t>(XFA_PacketType::Xmpmeta),
  XFA_XDPPACKET_Signature = 1
                            << static_cast<uint8_t>(XFA_PacketType::Signature),
  XFA_XDPPACKET_Stylesheet =
      1 << static_cast<uint8_t>(XFA_PacketType::Stylesheet),
  XFA_XDPPACKET_USER = 1 << static_cast<uint8_t>(XFA_PacketType::User),
  XFA_XDPPACKET_XDP = 1 << static_cast<uint8_t>(XFA_PacketType::Xdp)
};

enum XFA_XDPPACKET_FLAGS {
  XFA_XDPPACKET_FLAGS_COMPLETEMATCH = 1,
  XFA_XDPPACKET_FLAGS_PREFIXMATCH = 2,
  XFA_XDPPACKET_FLAGS_NOMATCH = 4,
  XFA_XDPPACKET_FLAGS_SUPPORTONE = 8,
  XFA_XDPPACKET_FLAGS_SUPPORTMANY = 16,
};

enum class XFA_AttributeEnum : uint16_t {
  Asterisk,
  Slash,
  Backslash,
  On,
  Tb,
  Up,
  MetaData,
  Delegate,
  PostSubmit,
  Name,
  Cross,
  Next,
  None,
  ShortEdge,
  Checksum_1mod10_1mod11,
  Height,
  CrossDiagonal,
  All,
  Any,
  ToRight,
  MatchTemplate,
  Dpl,
  Invisible,
  Fit,
  Width,
  PreSubmit,
  Ipl,
  FlateCompress,
  Med,
  Odd,
  Off,
  Pdf,
  Row,
  Top,
  Xdp,
  Xfd,
  Xml,
  Zip,
  Zpl,
  Visible,
  Exclude,
  MouseEnter,
  Pair,
  Filter,
  MoveLast,
  ExportAndImport,
  Push,
  Portrait,
  Default,
  StoredProc,
  StayBOF,
  StayEOF,
  PostPrint,
  UsCarrier,
  Right,
  PreOpen,
  Actual,
  Rest,
  TopCenter,
  StandardSymbol,
  Initialize,
  JustifyAll,
  Normal,
  Landscape,
  NonInteractive,
  MouseExit,
  Minus,
  DiagonalLeft,
  SimplexPaginated,
  Document,
  Warning,
  Auto,
  Below,
  BottomLeft,
  BottomCenter,
  Tcpl,
  Text,
  Grouping,
  SecureSymbol,
  PreExecute,
  DocClose,
  Keyset,
  Vertical,
  PreSave,
  PreSign,
  Bottom,
  ToTop,
  Verify,
  First,
  ContentArea,
  Solid,
  Pessimistic,
  DuplexPaginated,
  Round,
  Remerge,
  Ordered,
  Percent,
  Even,
  Exit,
  ToolTip,
  OrderedOccurrence,
  ReadOnly,
  Currency,
  Concat,
  Thai,
  Embossed,
  Formdata,
  Greek,
  Decimal,
  Select,
  LongEdge,
  Protected,
  BottomRight,
  Zero,
  ForwardOnly,
  DocReady,
  Hidden,
  Include,
  Dashed,
  MultiSelect,
  Inactive,
  Embed,
  Static,
  OnEntry,
  Cyrillic,
  NonBlank,
  TopRight,
  Hebrew,
  TopLeft,
  Center,
  MoveFirst,
  Diamond,
  PageOdd,
  Checksum_1mod10,
  Korean,
  AboveEmbedded,
  ZipCompress,
  Numeric,
  Circle,
  ToBottom,
  Inverted,
  Update,
  Isoname,
  Server,
  Position,
  MiddleCenter,
  Optional,
  UsePrinterSetting,
  Outline,
  IndexChange,
  Change,
  PageArea,
  Once,
  Only,
  Open,
  Caption,
  Raised,
  Justify,
  RefAndDescendants,
  Short,
  PageFront,
  Monospace,
  Middle,
  PrePrint,
  Always,
  Unknown,
  ToLeft,
  Above,
  DashDot,
  Gregorian,
  Roman,
  MouseDown,
  Symbol,
  PageEven,
  Sign,
  AddNew,
  Star,
  Optimistic,
  Rl_tb,
  MiddleRight,
  Maintain,
  Package,
  SimplifiedChinese,
  ToCenter,
  Back,
  Unspecified,
  BatchOptimistic,
  Bold,
  Both,
  Butt,
  Client,
  Checksum_2mod10,
  ImageOnly,
  Horizontal,
  Dotted,
  UserControl,
  DiagonalRight,
  ConsumeData,
  Check,
  Data,
  Down,
  SansSerif,
  Inline,
  TraditionalChinese,
  Warn,
  RefOnly,
  InteractiveForms,
  Word,
  Unordered,
  Required,
  ImportOnly,
  BelowEmbedded,
  Japanese,
  Full,
  Rl_row,
  Vietnamese,
  EastEuropeanRoman,
  MouseUp,
  ExportOnly,
  Clear,
  Click,
  Base64,
  Close,
  Host,
  Global,
  Blank,
  Table,
  Import,
  Custom,
  MiddleLeft,
  PostExecute,
  Radix,
  PostOpen,
  Enter,
  Ignore,
  Lr_tb,
  Fantasy,
  Italic,
  Author,
  ToEdge,
  Choice,
  Disabled,
  CrossHatch,
  DataRef,
  DashDotDot,
  Square,
  Dynamic,
  Manual,
  Etched,
  ValidationState,
  Cursive,
  Last,
  Left,
  Link,
  Long,
  InternationalCarrier,
  PDF1_3,
  PDF1_6,
  Serif,
  PostSave,
  Ready,
  PostSign,
  Arabic,
  Error,
  Urlencoded,
  Lowered
};

enum class XFA_Attribute : uint8_t {
  H = 0,
  W,
  X,
  Y,
  Id,
  To,
  LineThrough,
  HAlign,
  Typeface,
  BeforeTarget,
  Name,
  Next,
  DataRowCount,
  Break,
  VScrollPolicy,
  FontHorizontalScale,
  TextIndent,
  Context,
  TrayOut,
  Cap,
  Max,
  Min,
  Ref,
  Rid,
  Url,
  Use,
  LeftInset,
  Widows,
  Level,
  BottomInset,
  OverflowTarget,
  AllowMacro,
  PagePosition,
  ColumnWidths,
  OverflowLeader,
  Action,
  NonRepudiation,
  Rate,
  AllowRichText,
  Role,
  OverflowTrailer,
  Operation,
  Timeout,
  TopInset,
  Access,
  CommandType,
  Format,
  DataPrep,
  WidgetData,
  Abbr,
  MarginRight,
  DataDescription,
  EncipherOnly,
  KerningMode,
  Rotate,
  WordCharacterCount,
  Type,
  Reserve,
  TextLocation,
  Priority,
  Underline,
  ModuleWidth,
  Hyphenate,
  Listen,
  Delimiter,
  ContentType,
  StartNew,
  EofAction,
  AllowNeutral,
  Connection,
  BaselineShift,
  OverlinePeriod,
  FracDigits,
  Orientation,
  TimeStamp,
  PrintCheckDigit,
  MarginLeft,
  Stroke,
  ModuleHeight,
  TransferEncoding,
  Usage,
  Presence,
  RadixOffset,
  Preserve,
  AliasNode,
  MultiLine,
  Version,
  StartChar,
  ScriptTest,
  StartAngle,
  CursorType,
  DigitalSignature,
  CodeType,
  Output,
  BookendTrailer,
  ImagingBBox,
  ExcludeInitialCap,
  Force,
  CrlSign,
  Previous,
  PushCharacterCount,
  NullTest,
  RunAt,
  SpaceBelow,
  SweepAngle,
  NumberOfCells,
  LetterSpacing,
  LockType,
  PasswordChar,
  VAlign,
  SourceBelow,
  Inverted,
  Mark,
  MaxH,
  MaxW,
  Truncate,
  MinH,
  MinW,
  Initial,
  Mode,
  Layout,
  Server,
  EmbedPDF,
  OddOrEven,
  TabDefault,
  Contains,
  RightInset,
  MaxChars,
  Open,
  Relation,
  WideNarrowRatio,
  Relevant,
  SignatureType,
  LineThroughPeriod,
  Shape,
  TabStops,
  OutputBelow,
  Short,
  FontVerticalScale,
  Thickness,
  CommitOn,
  RemainCharacterCount,
  KeyAgreement,
  ErrorCorrectionLevel,
  UpsMode,
  MergeMode,
  Circular,
  PsName,
  Trailer,
  UnicodeRange,
  ExecuteType,
  DuplexImposition,
  TrayIn,
  BindingNode,
  BofAction,
  Save,
  TargetType,
  KeyEncipherment,
  CredentialServerPolicy,
  Size,
  InitialNumber,
  Slope,
  CSpace,
  ColSpan,
  Binding,
  Checksum,
  CharEncoding,
  Bind,
  TextEntry,
  Archive,
  Uuid,
  Posture,
  After,
  Orphans,
  QualifiedName,
  Usehref,
  Locale,
  Weight,
  UnderlinePeriod,
  Data,
  Desc,
  Numbered,
  DataColumnCount,
  Overline,
  UrlPolicy,
  AnchorType,
  LabelRef,
  BookendLeader,
  MaxLength,
  AccessKey,
  CursorLocation,
  DelayedOpen,
  Target,
  DataEncipherment,
  AfterTarget,
  Leader,
  Picker,
  From,
  BaseProfile,
  Aspect,
  RowColumnRatio,
  LineHeight,
  Highlight,
  ValueRef,
  MaxEntries,
  DataLength,
  Activity,
  Input,
  Value,
  BlankOrNotBlank,
  AddRevocationInfo,
  GenericFamily,
  Hand,
  Href,
  TextEncoding,
  LeadDigits,
  Permissions,
  SpaceAbove,
  CodeBase,
  Stock,
  IsNull,
  RestoreState,
  ExcludeAllCaps,
  FormatTest,
  HScrollPolicy,
  Join,
  KeyCertSign,
  Radius,
  SourceAbove,
  Override,
  ClassId,
  Disable,
  Scope,
  Match,
  Placement,
  Before,
  WritingScript,
  EndChar,
  Lock,
  Long,
  Intact,
  XdpContent,
  DecipherOnly,
  Unknown = 255,
};

enum class XFA_Element : int16_t {
  Unknown = -1,

  Ps,
  To,
  Ui,
  RecordSet,
  SubsetBelow,
  SubformSet,
  AdobeExtensionLevel,
  Typeface,
  Break,
  FontInfo,
  NumberPattern,
  DynamicRender,
  PrintScaling,
  CheckButton,
  DatePatterns,
  SourceSet,
  Amd,
  Arc,
  Day,
  Era,
  Jog,
  Log,
  Map,
  Mdp,
  BreakBefore,
  Oid,
  Pcl,
  Pdf,
  Ref,
  Uri,
  Xdc,
  Xdp,
  Xfa,
  Xsl,
  Zpl,
  Cache,
  Margin,
  KeyUsage,
  Exclude,
  ChoiceList,
  Level,
  LabelPrinter,
  CalendarSymbols,
  Para,
  Part,
  Pdfa,
  Filter,
  Present,
  Pagination,
  Encoding,
  Event,
  Whitespace,
  DefaultUi,
  DataModel,
  Barcode,
  TimePattern,
  BatchOutput,
  Enforce,
  CurrencySymbols,
  AddSilentPrint,
  Rename,
  Operation,
  Typefaces,
  SubjectDNs,
  Issuers,
  SignaturePseudoModel,
  WsdlConnection,
  Debug,
  Delta,
  EraNames,
  ModifyAnnots,
  StartNode,
  Button,
  Format,
  Border,
  Area,
  Hyphenation,
  Text,
  Time,
  Type,
  Overprint,
  Certificates,
  EncryptionMethods,
  SetProperty,
  PrinterName,
  StartPage,
  PageOffset,
  DateTime,
  Comb,
  Pattern,
  IfEmpty,
  SuppressBanner,
  OutputBin,
  Field,
  Agent,
  OutputXSL,
  AdjustData,
  AutoSave,
  ContentArea,
  EventPseudoModel,
  WsdlAddress,
  Solid,
  DateTimeSymbols,
  EncryptionLevel,
  Edge,
  Stipple,
  Attributes,
  VersionControl,
  Meridiem,
  ExclGroup,
  ToolTip,
  Compress,
  Reason,
  Execute,
  ContentCopy,
  DateTimeEdit,
  Config,
  Image,
  SharpxHTML,
  NumberOfCopies,
  BehaviorOverride,
  TimeStamp,
  Month,
  ViewerPreferences,
  ScriptModel,
  Decimal,
  Subform,
  Select,
  Window,
  LocaleSet,
  Handler,
  HostPseudoModel,
  Presence,
  Record,
  Embed,
  Version,
  Command,
  Copies,
  Staple,
  SubmitFormat,
  Boolean,
  Message,
  Output,
  PsMap,
  ExcludeNS,
  Assist,
  Picture,
  Traversal,
  SilentPrint,
  WebClient,
  LayoutPseudoModel,
  Producer,
  Corner,
  MsgId,
  Color,
  Keep,
  Query,
  Insert,
  ImageEdit,
  Validate,
  DigestMethods,
  NumberPatterns,
  PageSet,
  Integer,
  SoapAddress,
  Equate,
  FormFieldFilling,
  PageRange,
  Update,
  ConnectString,
  Mode,
  Layout,
  Sharpxml,
  XsdConnection,
  Traverse,
  Encodings,
  Template,
  Acrobat,
  ValidationMessaging,
  Signing,
  DataWindow,
  Script,
  AddViewerPreferences,
  AlwaysEmbed,
  PasswordEdit,
  NumericEdit,
  EncryptionMethod,
  Change,
  PageArea,
  SubmitUrl,
  Oids,
  Signature,
  ADBE_JSConsole,
  Caption,
  Relevant,
  FlipLabel,
  ExData,
  DayNames,
  SoapAction,
  DefaultTypeface,
  Manifest,
  Overflow,
  Linear,
  CurrencySymbol,
  Delete,
  Deltas,
  DigestMethod,
  InstanceManager,
  EquateRange,
  Medium,
  TextEdit,
  TemplateCache,
  CompressObjectStream,
  DataValue,
  AccessibleContent,
  TreeList,
  IncludeXDPContent,
  XmlConnection,
  ValidateApprovalSignatures,
  SignData,
  Packets,
  DatePattern,
  DuplexOption,
  Base,
  Bind,
  Compression,
  User,
  Rectangle,
  EffectiveOutputPolicy,
  ADBE_JSDebugger,
  Acrobat7,
  Interactive,
  Locale,
  CurrentPage,
  Data,
  Date,
  Desc,
  Encrypt,
  Draw,
  Encryption,
  MeridiemNames,
  Messaging,
  Speak,
  DataGroup,
  Common,
  Sharptext,
  PaginationOverride,
  Reasons,
  SignatureProperties,
  Threshold,
  AppearanceFilter,
  Fill,
  Font,
  Form,
  MediumInfo,
  Certificate,
  Password,
  RunScripts,
  Trace,
  Float,
  RenderPolicy,
  LogPseudoModel,
  Destination,
  Value,
  Bookend,
  ExObject,
  OpenAction,
  NeverEmbed,
  BindItems,
  Calculate,
  Print,
  Extras,
  Proto,
  DSigData,
  Creator,
  Connect,
  Permissions,
  ConnectionSet,
  Submit,
  Range,
  Linearized,
  Packet,
  RootElement,
  PlaintextMetadata,
  NumberSymbols,
  PrintHighQuality,
  Driver,
  IncrementalLoad,
  SubjectDN,
  CompressLogicalStructure,
  IncrementalMerge,
  Radial,
  Variables,
  TimePatterns,
  EffectiveInputPolicy,
  NameAttr,
  Conformance,
  Transform,
  LockDocument,
  BreakAfter,
  Line,
  List,
  Source,
  Occur,
  PickTrayByPDFSize,
  MonthNames,
  Severity,
  GroupParent,
  DocumentAssembly,
  NumberSymbol,
  Tagged,
  Items
};

enum class XFA_AttributeType : uint8_t {
  Enum,
  CData,
  Boolean,
  Integer,
  Measure,
};

struct XFA_SCRIPTHIERARCHY {
  uint16_t wAttributeStart;
  uint16_t wAttributeCount;
  int16_t wParentIndex;
};

#define XFA_PROPERTYFLAG_OneOf 0x01
#define XFA_PROPERTYFLAG_DefaultOneOf 0x02

struct XFA_AttributeEnumInfo {
  uint32_t uHash;
  const wchar_t* pName;
  XFA_AttributeEnum eName;
};

enum class XFA_Unit : uint8_t {
  Percent = 0,
  Em,
  Pt,
  In,
  Pc,
  Cm,
  Mm,
  Mp,

  Unknown = 255,
};

enum class XFA_ScriptType : uint8_t {
  Basic,
  Object,
};

#endif  // XFA_FXFA_FXFA_BASIC_H_
