#pragma once

#include <Arduino.h>

#include <vector>

namespace model {

// A celebration's text as the content host sends it: a summary that fits the
// glass, and the post in headings and paragraphs for whoever reads on.
struct TextBlock {
  bool heading = false;
  String text;
};

struct CelebrationText {
  bool found = false;
  String title;
  String summary;
  std::vector<TextBlock> body;
  bool more = false;  // the post goes on in the app
};

// Parses the host's JSON ({ v, title, summary, body: [{h}|{p}], more }).
// False, and `out` empty, when it is not that.
bool parseCelebrationText(const char* json, size_t length, CelebrationText& out);

// The name a celebration's files are kept under: sixteen hex digits of a hash
// of its slug. Slugs run to eighty characters and the file system stops at
// sixty-four.
String contentKey(const String& slug);

// A firmware on offer, as the site's manifest describes it for one board.
struct FirmwareOffer {
  bool found = false;
  String version;    // "0.4.1"
  String path;       // "/firmware/0.4.1/amoled175c.bin", on the site
  uint32_t size = 0;
  String sha256;     // of the file, 64 hex digits
  String signature;  // base64 of an ECDSA signature over that hash; may be empty
  String notes;      // what it brings, in the owner's language
};

// Parses /firmware/manifest.json and picks the build for `board`. False when
// the manifest is malformed, has nothing for the board, or describes a file
// the compass would not accept (a path outside /firmware/, a size that is
// not a firmware's, a hash that is not a hash).
bool parseManifest(const char* json, size_t length, const char* board, const String& language, FirmwareOffer& out);

// Version numbers as "0.4.1": true when `candidate` is newer than `current`.
// Anything that is not three numbers is not newer than anything.
bool versionNewer(const char* candidate, const char* current);

}  // namespace model
