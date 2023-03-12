/**
 * @file
 * @author Max Godefroy
 * @date 10/10/2022.
 */

#include "DataCooker.hpp"

namespace KryneEngine::Tools
{
    DataCooker::DataCooker(u16 _workerCount)
        : m_fibersManager(_workerCount)
    {

    }
}