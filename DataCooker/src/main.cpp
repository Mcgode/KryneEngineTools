/**
 * @file
 * @author Max Godefroy
 * @date 10/10/2022.
 */

#include <Common/Assert.hpp>
#include "DataCooker.hpp"

using namespace KryneEngine;

int main(s32 _argc, const char** _argv)
{
    Assert(_argc > 0);

    const eastl::string_view dirPath = _argv[0];

    Tools::DataCooker dataCooker(dirPath);
}
