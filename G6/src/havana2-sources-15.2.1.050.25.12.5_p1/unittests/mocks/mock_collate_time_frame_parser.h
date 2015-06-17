#ifndef _MOCK_COLLATE_TIME_FRAME_PARSER_H_
#define _MOCK_COLLATE_TIME_FRAME_PARSER_H_

#include "gmock/gmock.h"
#include "frame_parser.h"

class MockCollateTimeFrameParser_c : public CollateTimeFrameParserInterface_c {
  public:
    MockCollateTimeFrameParser_c();
    virtual ~MockCollateTimeFrameParser_c();

    MOCK_CONST_METHOD0(GetMaxReferenceCount,
                       unsigned int(void));
    MOCK_METHOD0(ResetCollatedHeaderState,
                 FrameParserStatus_t(void));
    MOCK_METHOD1(RequiredPresentationLength,
                 unsigned int(unsigned char StartCode));
    MOCK_METHOD3(PresentCollatedHeader,
                 FrameParserStatus_t(unsigned char         StartCode,
                                     unsigned char        *HeaderBytes,
                                     FrameParserHeaderFlag_t  *Flags));
};

#endif
