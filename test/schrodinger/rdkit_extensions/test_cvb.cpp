/* -------------------------------------------------------------------------
 * Copyright Schrodinger LLC, All Rights Reserved.
 --------------------------------------------------------------------------- */

#define BOOST_TEST_MODULE test_file_stream

#include <boost/test/data/test_case.hpp>
#include <boost/test/unit_test.hpp>
#include <cstdio>
#include <sstream>
#include <ios>

#include "schrodinger/rdkit_extensions/file_format.h"
#include "schrodinger/rdkit_extensions/file_stream.h"
#include "test_common.h"

using namespace schrodinger::rdkit_extensions;
namespace bdata = boost::unit_test::data;

BOOST_AUTO_TEST_CASE(test_cvb)
{
    auto fname = testfile_path("methane.maegz");
    std::ifstream is(fname);
    std::string buffer(std::istreambuf_iterator<char>(is), {});

    // MAESTRO formatted file should have a title
    auto decompressed_string =
        get_decompressed_string(buffer, CompressionType::GZIP);
    BOOST_TEST(decompressed_string.find("s_m_title") != std::string::npos);

    decompressed_string = get_decompressed_string(buffer);
    BOOST_TEST(decompressed_string.find("s_m_title") != std::string::npos);
}
