// Range v3 library
//
//  Copyright Eric Niebler 2014-present
//
//  Use, modification and distribution is subject to the
//  Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// Project home: https://github.com/ericniebler/range-v3

#include <string>
#include <vector>
#include <iterator>
#include <functional>
#include <range/v3/core.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/counted.hpp>
#include <range/v3/view/reverse.hpp>
#include <range/v3/span.hpp>
#include <range/v3/view/zip.hpp>
#include <range/v3/algorithm/move.hpp>
#include <range/v3/utility/copy.hpp>
#include "../simple_test.hpp"
#include "../test_utils.hpp"

struct is_odd
{
    bool operator()(int i) const
    {
        return (i % 2) == 1;
    }
};

// https://github.com/ericniebler/range-v3/issues/996
void bug_996()
{
    std::vector<int> buff(12, -1);
    ::ranges::span<int> sp(buff.data(), 12);

    auto x = ::ranges::view::transform(sp, [](int a) { return a > 3 ? a : 42; });
    auto y = ::ranges::view::transform(x, sp, [](int a, int b) { return a + b; });
    auto rng = ::ranges::view::transform(y, [](int a) { return a + 1; });
    (void)rng;
}

void test_size()
{
    using namespace ranges;

    std::vector<int> vec = {1,2,3};

    // Test with functor, not lambda.
    // Since lambda default constructible from C++20 only.
    struct Op
    {
        int operator()(int a) const
        {
            return a + 1;
        }
    };

    auto t1 = vec | view::transform(Op{});
    auto t2 = t1  | view::transform(Op{});
    auto t3 = t2  | view::transform(Op{});

    ::check_equal(t3.front(), vec.front() + 3);

    std::cout << sizeof(vec.begin()) << std::endl;
    std::cout << sizeof(t1.begin()) << std::endl;
    std::cout << sizeof(t2.begin()) << std::endl;

    // Output is: 8 8 16


    /*static_assert(
        sizeof(vec.begin()) == sizeof(t1.begin()) &&
        sizeof(t1.begin())  == sizeof(t2.begin()) &&
        sizeof(t2.begin())  == sizeof(t3.begin()),
        "iterators sizes should not grow!" );*/
}

int main()
{
    test_size();

    using namespace ranges;

    int rgi[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    auto && rng = rgi | view::transform(is_odd());
    has_type<int &>(*begin(rgi));
    has_type<bool>(*begin(rng));
    models<concepts::SizedView>(aux::copy(rng));
    models<concepts::RandomAccessView>(aux::copy(rng));
    ::check_equal(rng, {true, false, true, false, true, false, true, false, true, false});

    std::pair<int, int> rgp[] = {{1,1}, {2,2}, {3,3}, {4,4}, {5,5}, {6,6}, {7,7}, {8,8}, {9,9}, {10,10}};
    auto && rng2 = rgp | view::transform(&std::pair<int,int>::first);
    has_type<int &>(*begin(rng2));
    CONCEPT_ASSERT(Same<range_value_type_t<decltype(rng2)>, int>());
    CONCEPT_ASSERT(Same<decltype(iter_move(begin(rng2))), int &&>());
    models<concepts::BoundedView>(aux::copy(rng2));
    models<concepts::SizedView>(aux::copy(rng2));
    models<concepts::RandomAccessView>(aux::copy(rng2));
    ::check_equal(rng2, {1,2,3,4,5,6,7,8,9,10});
    ::check_equal(rng2 | view::reverse, {10,9,8,7,6,5,4,3,2,1});
    CHECK(&*begin(rng2) == &rgp[0].first);
    CHECK(rng2.size() == 10u);

    auto && rng3 = view::counted(rgp, 10) | view::transform(&std::pair<int,int>::first);
    has_type<int &>(*begin(rng3));
    models<concepts::BoundedView>(aux::copy(rng3));
    models<concepts::SizedView>(aux::copy(rng3));
    models<concepts::RandomAccessView>(aux::copy(rng3));
    ::check_equal(rng3, {1,2,3,4,5,6,7,8,9,10});
    CHECK(&*begin(rng3) == &rgp[0].first);
    CHECK(rng3.size() == 10u);

    auto && rng4 = view::counted(forward_iterator<std::pair<int, int>*>{rgp}, 10)
                      | view::transform(&std::pair<int,int>::first);
    has_type<int &>(*begin(rng4));
    models_not<concepts::BoundedView>(aux::copy(rng4));
    models<concepts::SizedView>(aux::copy(rng4));
    models<concepts::ForwardView>(aux::copy(rng4));
    models_not<concepts::BidirectionalView>(aux::copy(rng4));
    ::check_equal(rng4, {1,2,3,4,5,6,7,8,9,10});
    CHECK(&*begin(rng4) == &rgp[0].first);
    CHECK(rng4.size() == 10u);

    counted_iterator<forward_iterator<std::pair<int, int>*>> i = begin(rng4).base();
    (void)i;

    // Test transform with a mutable lambda
    int cnt = 100;
    auto mutable_rng = view::transform(rgi, [cnt](int) mutable { return cnt++;});
    ::check_equal(mutable_rng, {100,101,102,103,104,105,106,107,108,109});
    CHECK(cnt == 100);
    CONCEPT_ASSERT(View<decltype(mutable_rng)>());
    CONCEPT_ASSERT(!View<decltype(mutable_rng) const>());

    // Test iter_transform by transforming a zip view to select one element.
    {
        auto v0 = to_<std::vector<MoveOnlyString>>({"a","b","c"});
        auto v1 = to_<std::vector<MoveOnlyString>>({"x","y","z"});

        auto rng = view::zip(v0, v1);
        ::models<concepts::RandomAccessRange>(rng);

        std::vector<MoveOnlyString> res;
        using R = decltype(rng);
        using I = iterator_t<R>;
        // Needlessly verbose -- a simple transform would do the same, but this
        // is an interesting test.
        auto proj = overload(
            [](I i) -> MoveOnlyString& {return (*i).first;},
            [](copy_tag, I) -> MoveOnlyString {return {};},
            [](move_tag, I i) -> MoveOnlyString&& {return std::move((*i).first);}
        );
        auto rng2 = rng | view::iter_transform(proj);
        move(rng2, ranges::back_inserter(res));
        ::check_equal(res, {"a","b","c"});
        ::check_equal(v0, {"","",""});
        ::check_equal(v1, {"x","y","z"});
        using R2 = decltype(rng2);
        CONCEPT_ASSERT(Same<range_value_type_t<R2>, MoveOnlyString>());
        CONCEPT_ASSERT(Same<range_reference_t<R2>, MoveOnlyString &>());
        CONCEPT_ASSERT(Same<range_rvalue_reference_t<R2>, MoveOnlyString &&>());
    }

    // two range transform
    {
        auto v0 = to_<std::vector<std::string>>({"a","b","c"});
        auto v1 = to_<std::vector<std::string>>({"x","y","z"});

        auto rng = view::transform(v0, v1, [](std::string& s0, std::string& s1){return std::tie(s0, s1);});
        using R = decltype(rng);
        CONCEPT_ASSERT(Same<range_value_type_t<R>, std::tuple<std::string&, std::string&>>());
        CONCEPT_ASSERT(Same<range_reference_t<R>, std::tuple<std::string&, std::string&>>());
        CONCEPT_ASSERT(Same<range_rvalue_reference_t<R>, std::tuple<std::string&, std::string&>>());

        using T = std::tuple<std::string, std::string>;
        ::check_equal(rng, {T{"a","x"}, T{"b","y"}, T{"c","z"}});
    }

    // two range indirect transform
    {
        auto v0 = to_<std::vector<std::string>>({"a","b","c"});
        auto v1 = to_<std::vector<std::string>>({"x","y","z"});
        using I = std::vector<std::string>::iterator;

        auto fun = overload(
            [](I i, I j)           { return std::tie(*i, *j); },
            [](copy_tag, I, I)     { return std::tuple<std::string, std::string>{}; },
            [](move_tag, I i, I j) { return common_tuple<std::string&&, std::string&&>{
                std::move(*i), std::move(*j)}; } );

        auto rng = view::iter_transform(v0, v1, fun);
        using R = decltype(rng);
        CONCEPT_ASSERT(Same<range_value_type_t<R>, std::tuple<std::string, std::string>>());
        CONCEPT_ASSERT(Same<range_reference_t<R>, std::tuple<std::string&, std::string&>>());
        CONCEPT_ASSERT(Same<range_rvalue_reference_t<R>, common_tuple<std::string&&, std::string&&>>());

        using T = std::tuple<std::string, std::string>;
        ::check_equal(rng, {T{"a","x"}, T{"b","y"}, T{"c","z"}});
    }

    // move-only input view transform
    {
        auto rng = debug_input_view<int const>{rgi} | view::transform(is_odd{});
        ::check_equal(rng, {true, false, true, false, true, false, true, false, true, false});
    }

    // two move-only input view transform
    {
        auto v0 = to_<std::vector<std::string>>({"a","b","c"});
        auto v1 = to_<std::vector<std::string>>({"x","y","z"});

        auto r0 = debug_input_view<std::string>{v0.data(), distance(v0)};
        auto r1 = debug_input_view<std::string>{v1.data(), distance(v1)};
        auto rng = view::transform(std::move(r0), std::move(r1),
            [](std::string &s0, std::string &s1){ return std::tie(s0, s1); });
        using R = decltype(rng);
        CONCEPT_ASSERT(Same<range_value_type_t<R>, std::tuple<std::string &, std::string &>>());
        CONCEPT_ASSERT(Same<range_reference_t<R>, std::tuple<std::string &, std::string &>>());
        CONCEPT_ASSERT(Same<range_rvalue_reference_t<R>, std::tuple<std::string &, std::string &>>());

        using T = std::tuple<std::string, std::string>;
        ::check_equal(rng, {T{"a","x"}, T{"b","y"}, T{"c","z"}});
    }

    return test_result();
}
