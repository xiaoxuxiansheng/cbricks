#include <gtest/gtest.h>

#include "../sync/thread.h"
#include "../base/sys.h"

namespace cbricks{namespace test{

class ThreadTest : testing::Test{
protected:
    pid_t threadId1 = 0;
    pid_t threadId2 = 0;

    virtual void SetUp() override {
    sync::Thread::ptr threadPtr1(new sync::Thread([this]{
       this->threadId1 = base::GetThreadId();
    },"test1"));

    sync::Thread::ptr threadPtr2(new sync::Thread([this]{
        this->threadId2 = base::GetThreadId();
    },"test2"));

    threadPtr1->join();
    threadPtr2->join();
  }

  virtual void TearDown() override {
  }

};

TEST_F(ThreadTest,GETID){
    ASSERT_NE(threadId1,0);
    ASSERT_NE(threadId2,0);
    ASSERT_NE(threadId1,threadId2);
}

int main(int argc, char** argv){
    ::testing::InitGoogleTest(&argc,argv);
    return RUN_ALL_TESTS();
}


}}