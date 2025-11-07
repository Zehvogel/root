#include "ROOT/RDataFrame.hxx"
#include "gtest/gtest.h"

TEST(DefineMulti, DefineMultipleColumns)
{
   ROOT::RDataFrame df(10);
   
   // Define two columns at once
   auto df2 = df.Define("x", []() { return 1.0; })
                .Define("y", []() { return 2.0; })
                .Define({"x2", "y2"}, [](double x, double y) { 
                   return ROOT::RVec<double>{x * x, y * y}; 
                }, {"x", "y"});
   
   auto x2_vals = df2.Take<double>("x2");
   auto y2_vals = df2.Take<double>("y2");
   
   EXPECT_EQ(x2_vals->size(), 10UL);
   EXPECT_EQ(y2_vals->size(), 10UL);
   
   for (auto v : *x2_vals) {
      EXPECT_DOUBLE_EQ(v, 1.0);
   }
   
   for (auto v : *y2_vals) {
      EXPECT_DOUBLE_EQ(v, 4.0);
   }
}

TEST(DefineMulti, DefineThreeColumns)
{
   ROOT::RDataFrame df(5);
   
   auto df2 = df.Define("a", []() { return 3.0; })
                .Define("b", []() { return 4.0; })
                .Define({"sum", "diff", "prod"}, [](double a, double b) {
                   return ROOT::RVec<double>{a + b, a - b, a * b};
                }, {"a", "b"});
   
   auto sum_vals = df2.Take<double>("sum");
   auto diff_vals = df2.Take<double>("diff");
   auto prod_vals = df2.Take<double>("prod");
   
   for (auto v : *sum_vals) {
      EXPECT_DOUBLE_EQ(v, 7.0);
   }
   
   for (auto v : *diff_vals) {
      EXPECT_DOUBLE_EQ(v, -1.0);
   }
   
   for (auto v : *prod_vals) {
      EXPECT_DOUBLE_EQ(v, 12.0);
   }
}

TEST(DefineMulti, DefineWithInitializerList)
{
   ROOT::RDataFrame df(3);
   
   auto df2 = df.Define("x", []() { return 5.0; })
                .Define({"double_x", "triple_x"}, [](double x) {
                   return ROOT::RVec<double>{x * 2, x * 3};
                }, {"x"});
   
   auto double_x = df2.Take<double>("double_x");
   auto triple_x = df2.Take<double>("triple_x");
   
   for (auto v : *double_x) {
      EXPECT_DOUBLE_EQ(v, 10.0);
   }
   
   for (auto v : *triple_x) {
      EXPECT_DOUBLE_EQ(v, 15.0);
   }
}

TEST(DefineMulti, DefineMultiWithSlot)
{
   ROOT::RDataFrame df(10);
   
   auto df2 = df.Define("x", []() { return 2.0; })
                .DefineSlot({"a", "b"}, [](unsigned int slot, double x) {
                   return ROOT::RVec<double>{x + slot, x * slot};
                }, {"x"});
   
   // Just check that it compiles and runs
   auto count = df2.Count();
   EXPECT_EQ(*count, 10UL);
}

TEST(DefineMulti, CheckWrongNumberOfReturnValues)
{
   ROOT::RDataFrame df(1);
   
   auto df2 = df.Define({"a", "b", "c"}, []() {
      // Returns only 2 values but 3 columns are expected
      return ROOT::RVec<double>{1.0, 2.0};
   }, {});
   
   // Should throw when trying to access the results
   EXPECT_THROW(df2.Take<double>("a"), std::runtime_error);
}
