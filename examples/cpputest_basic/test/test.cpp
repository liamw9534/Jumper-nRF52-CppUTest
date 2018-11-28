#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "production_code.h"

class ClassFromProductionCodeMock : public ClassFromProductionCode
{
public:
    virtual void productionFunction()
    {
        mock().actualCall("productionFunction");
    }
};


TEST_GROUP(FirstTestGroup)
{
};

TEST(FirstTestGroup, FirstTest)
{
   FAIL("Fail me!");
}

TEST(FirstTestGroup, SecondTest)
{
    STRCMP_EQUAL("hello", "world");
}

TEST_GROUP(ProductionMock)
{
    void teardown()
    {
        mock().clear();
    }
};

TEST(ProductionMock, MockTest)
{
    static ClassFromProductionCodeMock m;

    mock().expectNCalls(2, "productionFunction");
    m.productionFunction();  // Should fail as only called once
    mock().checkExpectations();
}
