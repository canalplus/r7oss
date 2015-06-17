/************************************************************************
COPYRIGHT (C) STMicroelectronics 2005

Source file name : havana_factory.cpp
Author :           Julian

Implementation of the class factory module for havana.


Date        Modification                                    Name
----        ------------                                    --------
16-Feb-07   Created                                         Julian

************************************************************************/

#include "player.h"
#include "havana_player.h"
#include "havana_factory.h"

HavanaFactory_c::HavanaFactory_c (void)
{
    //HAVANA_DEBUG("\n");
}
HavanaFactory_c::~HavanaFactory_c (void)
{
    //HAVANA_DEBUG("\n");
}
HavanaStatus_t HavanaFactory_c::Init   (class HavanaFactory_c*  FactoryList,
                                        const char*             Id,
                                        const char*             SubId,
                                        PlayerStreamType_t      StreamType,
                                        PlayerComponent_t       Component,
                                        unsigned int            Version,
                                        void*                  (*NewFactory)     (void))
{
    //HAVANA_DEBUG("\n");

    NextFactory         = FactoryList;
    this->Id            = Id;
    this->SubId         = SubId;
    PlayerStreamType    = StreamType;
    PlayerComponent     = Component;
    FactoryVersion      = Version;
    Factory             = NewFactory;

    return HavanaNoError;
}
HavanaStatus_t HavanaFactory_c::ReLink (class HavanaFactory_c*  Next)
{
    HAVANA_DEBUG("\n");
    NextFactory         = Next;
    return HavanaNoError;
}
//{{{  doxynote
/// \brief Determine whether we are able to build a component of the required type
/// \param Id           Identifier use to distinguish component from other similar components
///                     e.g. mpeg2 for a frame parser
/// \param SubId        Extra identifier use to distinguish component from other similar components
///                     e.g. mpeg2 for an audio pes collator from an lpcm audio pes collator
/// \param StreamType   Player type to indicate audio or video
/// \param Component    Player class to be constructed
/// \return             true if can build the componenet, false if not
//}}}  
bool HavanaFactory_c::CanBuild         (const char*             Id,
                                        const char*             SubId,
                                        PlayerStreamType_t      StreamType,
                                        PlayerComponent_t       Component)
{
//    HAVANA_ERROR("I am %s, %s, %x, %x - Looking for %s, %s, %x, %x\n", this->Id, this->SubId, PlayerStreamType, PlayerComponent,
//                                                                       Id, SubId, StreamType, Component);

    if ((StreamType == PlayerStreamType) && (Component == PlayerComponent) && (strcmp (Id, this->Id) == 0))
        return ((strcmp (SubId, this->SubId) == 0) ||
                (strcmp (SubId, FACTORY_ANY_ID) == 0) ||
                (strcmp (FACTORY_ANY_ID, this->SubId) == 0));
    else
        return false;
}
HavanaStatus_t HavanaFactory_c::Build  (void**  Class)
{
    //HAVANA_DEBUG("\n");

    *Class     = Factory ();
    if (*Class == NULL)
        return HavanaNoMemory;

    return HavanaNoError;
}
class HavanaFactory_c* HavanaFactory_c::Next   (void)
{
    //HAVANA_DEBUG("\n");

    return NextFactory;
}
unsigned int HavanaFactory_c::Version  (void)
{
    HAVANA_DEBUG("\n");

    return FactoryVersion;
}

