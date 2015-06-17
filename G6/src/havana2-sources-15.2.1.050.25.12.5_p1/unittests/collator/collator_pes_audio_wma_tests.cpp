#include "player_generic.h"
#include "player_playback.h"
#include "player_stream.h"
#include "collator_ctest.h"
#include "collator_pes_audio_wma.h"

using ::testing::InSequence;

class Collator_PesAudioWma_cTest: public Collator_cTest {
  protected:
    Collator_PesAudioWma_cTest() : Collator_cTest(mCollator_PesAudioWma, StreamTypeAudio) {}

    Collator_PesAudioWma_c mCollator_PesAudioWma;

};


TEST_F(Collator_PesAudioWma_cTest, test) {
}

