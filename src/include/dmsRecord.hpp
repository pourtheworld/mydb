#ifndef DMSRECORD_HPP_
#define DMSRECORD_HPP_

typedef unsigned int PAGEID;
typedef unsigned int SLOTID;

//每一个Record都是通过RID表示的，RID又由页id和slotid组成。
struct dmsRecordID
{
    PAGEID _pageID;
    SLOTID _slotID;
};


#endif