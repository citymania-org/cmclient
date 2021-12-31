#ifndef CM_EXPORT_HPP
#define CM_EXPORT_HPP

#include <string>

namespace citymania {

void ExportOpenttdData(const std::string &filename);
void ExportFrameSprites();
void StartRecording();
void StopRecording();

} // namespace citymania

#endif


