#include "PaletteManager.h"
#include "StringUtils.h"

#include <fstream>
#include <sstream>

using namespace std;


#define PALETTES_FILE_SUFFIX "_palettes.txt"


uint32_t PaletteManager::computeHighlightColor ( uint32_t color )
{
    const uint32_t r = ( color & 0xFF );
    const uint32_t g = ( color & 0xFF00 ) >> 8;
    const uint32_t b = ( color & 0xFF0000 ) >> 16;

    const uint32_t absDivColor2 = 3 * 220 * 220;

    if ( r * r + g * g + b * b > absDivColor2 )
    {
        return ( color & 0xFF000000 ) | 0x111111; // Dark grey
    }
    else
    {
        return ( color & 0xFF000000 ) | 0xFFFFFF; // White
    }
}

void PaletteManager::cache ( const uint32_t **allPaletteData )
{
    for ( uint32_t i = 0; i < originals.size(); ++i )
    {
        for ( uint32_t j = 0; j < originals[i].size(); ++j )
        {
            originals[i][j] = SWAP_R_AND_B ( allPaletteData[i][j] );
        }
    }
}

void PaletteManager::apply ( uint32_t **allPaletteData ) const
{
    for ( uint32_t i = 0; i < originals.size(); ++i )
    {
        for ( uint32_t j = 0; j < originals[i].size(); ++j )
        {
            allPaletteData[i][j] = ( allPaletteData[i][j] & 0xFF000000 )
                                   | ( 0xFFFFFF & SWAP_R_AND_B ( get ( i, j ) ) );
        }
    }
}

void PaletteManager::cache ( const uint32_t *allPaletteData )
{
    for ( uint32_t i = 0; i < originals.size(); ++i )
    {
        for ( uint32_t j = 0; j < originals[i].size(); ++j )
        {
            originals[i][j] = SWAP_R_AND_B ( allPaletteData [ i * 256 + j ] );
        }
    }
}

void PaletteManager::apply ( uint32_t *allPaletteData ) const
{
    for ( uint32_t i = 0; i < 36; ++i )
    {
        for ( uint32_t j = 0; j < 256; ++j )
        {
            allPaletteData [ i * 256 + j ] = ( allPaletteData [ i * 256 + j ] & 0xFF000000 )
                                             | ( 0xFFFFFF & SWAP_R_AND_B ( get ( i, j ) ) );
        }
    }
}

void PaletteManager::apply ( uint32_t paletteNumber, uint32_t *singlePaletteData ) const
{
    for ( uint32_t j = 0; j < 256; ++j )
    {
        singlePaletteData[j] = ( singlePaletteData [j] & 0xFF000000 )
                               | ( 0xFFFFFF & SWAP_R_AND_B ( get ( paletteNumber, j ) ) );
    }
}

uint32_t PaletteManager::getOriginal ( uint32_t paletteNumber, uint32_t colorNumber ) const
{
    return originals[paletteNumber][colorNumber];
}

uint32_t PaletteManager::get ( uint32_t paletteNumber, uint32_t colorNumber ) const
{
    auto it = palettes.find ( paletteNumber );

    if ( it != palettes.end() )
    {
        const auto jt = it->second.find ( colorNumber );

        if ( jt != it->second.end() )
            return jt->second;
    }

    return getOriginal ( paletteNumber, colorNumber );
}

void PaletteManager::set ( uint32_t paletteNumber, uint32_t colorNumber, uint32_t color )
{
    palettes[paletteNumber][colorNumber] = color;

#ifndef DISABLE_SERIALIZATION
    invalidate();
#endif
}

void PaletteManager::clear ( uint32_t paletteNumber, uint32_t colorNumber )
{
    const auto it = palettes.find ( paletteNumber );

    if ( it != palettes.end() )
    {
        it->second.erase ( colorNumber );

        if ( it->second.empty() )
            clear ( paletteNumber );
    }

#ifndef DISABLE_SERIALIZATION
    invalidate();
#endif
}

void PaletteManager::clear ( uint32_t paletteNumber )
{
    palettes.erase ( paletteNumber );

#ifndef DISABLE_SERIALIZATION
    invalidate();
#endif
}

void PaletteManager::clear()
{
    palettes.clear();

#ifndef DISABLE_SERIALIZATION
    invalidate();
#endif
}

bool PaletteManager::empty() const
{
    return palettes.empty();
}

bool PaletteManager::save ( const string& folder, const string& charaName ) const
{
    ofstream fout ( ( folder + charaName + PALETTES_FILE_SUFFIX ).c_str() );
    bool good = fout.good();

    if ( good )
    {
        fout << "\n######## " << charaName << " palettes ########" << endl;
        fout << endl;
        fout << "# Color are specified as 'color_NUMBER_ID=#HEX_CODE'" << endl;
        fout << "# Lines starting with # are ignored" << endl;

        for ( auto it = palettes.cbegin(); it != palettes.cend(); ++it )
        {
            fout << format ( "\n### Color %02d start ###\n", it->first + 1 ) << endl;

            for ( const auto& kv : it->second )
            {
                const string line = format ( "color_%02d_%03d=#%06X", it->first + 1, kv.first + 1, kv.second );

                fout << line << endl;
            }

            fout << format ( "\n#### Color %02d end ####\n", it->first + 1 ) << endl;
        }

        good = fout.good();
    }

    fout.close();
    return good;
}

bool PaletteManager::load ( const string& folder, const string& charaName )
{
    ifstream fin ( ( folder + charaName + PALETTES_FILE_SUFFIX ).c_str() );
    bool good = fin.good();

    if ( good )
    {
        string line;

        while ( getline ( fin, line ) )
        {
            if ( line.empty() || line[0] == '#' )
                continue;

            vector<string> parts = split ( line, "=" );

            if ( parts.size() != 2 || parts[0].empty() || parts[1].empty() || parts[1][0] != '#' )
                continue;

            vector<string> prefix = split ( parts[0], "_" );

            if ( prefix.size() != 3 || prefix[0] != "color" || prefix[1].empty() || prefix[2].empty() )
                continue;

            const uint32_t paletteNumber = lexical_cast<uint32_t> ( prefix[1] ) - 1;
            const uint32_t colorNumber = lexical_cast<uint32_t> ( prefix[2] ) - 1;

            uint32_t color;

            stringstream ss ( parts[1].substr ( 1 ) );
            ss >> hex >> color;

            palettes[paletteNumber][colorNumber] = color;
        }

#ifndef DISABLE_SERIALIZATION
        invalidate();
#endif
    }

    fin.close();
    return good;
}
