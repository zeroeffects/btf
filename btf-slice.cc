#define BTF_IMPLEMENTATION
#include "btf.hh"

typedef int8_t  tga_byte;
typedef int16_t tga_short;
typedef int32_t   tga_long;
typedef char    tga_ascii;

#if !defined(_MSC_VER) && !defined(MINGW_CROSS)
#define ATTR_PACKED __attribute__((packed))
#else
#define ATTR_PACKED
#endif

#if defined(_MSC_VER) || defined(MINGW_CROSS)
#   pragma pack(push, 1)
#endif
struct ATTR_PACKED TGAHeader
{
    tga_byte    IDLength;
    tga_byte    ColorMapType;
    tga_byte    ImageType;

    tga_short   CMS_FirstEntryIndex;
    tga_short   CMS_ColorMapLength;
    tga_byte    CMS_ColorMapEntrySize;

    tga_short   IS_XOrigin;
    tga_short   IS_YOrigin;
    tga_short   IS_ImageWidth;
    tga_short   IS_ImageHeight;
    tga_byte    IS_PixelDepth;
    tga_byte    IS_ImageDescriptor;
};
#if defined(_MSC_VER) || defined(MINGW_CROSS)
#   pragma pack(pop)
#endif

enum TGAImageType
{
    TGA_NO_IMAGE_DATA = 0,
    TGA_UNCOMPRESSED_COLOR_MAPPED = 1,
    TGA_UNCOMPRESSED_TRUE_COLOR = 2,
    TGA_UNCOMPRESSED_BLACK_AND_WHITE = 3,
    TGA_RUN_LENGTH_COLOR_MAPPED = 9,
    TGA_RUN_LENGTH_TRUE_COLOR = 10,
    TGA_RUN_LENGTH_BLACK_AND_WHITE = 11
};

enum TGAMask
{
    TGA_IMAGE_TYPE_RLE_MASK = 1 << 3,
    TGA_RIGHT_BIT_MASK = 1 << 4,
    TGA_TOP_BIT_MASK = 1 << 5,
    TGA_RLE_MASK = 1 << 7
};

bool SaveTGAImage(uint16_t width, uint16_t height, const void* data, const char* filename)
{
    std::fstream fs(filename, std::ios::out | std::ios::binary);
    if(!fs)
    {
        return false;
    }

    TGAHeader tga_header;
    tga_header.IDLength = 0;
    tga_header.ColorMapType = 0;

    tga_header.CMS_FirstEntryIndex = 0;
    tga_header.CMS_ColorMapLength = 0;
    tga_header.CMS_ColorMapEntrySize = 0;

    tga_header.IS_XOrigin = 0;
    tga_header.IS_YOrigin = 0;
    tga_header.IS_ImageWidth = width;
    tga_header.IS_ImageHeight = height;
    tga_header.IS_ImageDescriptor = 0;

    std::unique_ptr<char[]> interm;
    const char* src_data = reinterpret_cast<const char*>(data);

    size_t image_size = tga_header.IS_ImageWidth*tga_header.IS_ImageHeight;

    tga_header.ImageType = TGA_UNCOMPRESSED_TRUE_COLOR;
    tga_header.IS_PixelDepth = 32;

    interm = std::unique_ptr<char[]>(new char[image_size*sizeof(uint32_t)]);
    auto *dst = reinterpret_cast<char*>(interm.get());
    auto *src = reinterpret_cast<const char*>(data);
    for(auto *dst_end = dst + image_size*sizeof(uint32_t); dst != dst_end; dst += sizeof(uint32_t))
    {
        dst[2] = *(src++);
        dst[1] = *(src++);
        dst[0] = *(src++);
        dst[3] = *(src++);
    }
    src_data = interm.get();

    fs.write(reinterpret_cast<const char*>(&tga_header), sizeof(tga_header));
    fs.write(src_data, tga_header.IS_ImageWidth*tga_header.IS_ImageHeight*tga_header.IS_PixelDepth/8);
    return true;
}

inline float ConvertLinearToSLuminance(float lum)
{
    if(lum <= 0.0031308f)
        return lum * 12.92f;
    else
        return 1.055f * powf(lum, 1.0f / 2.4f) - 0.055f;
}

inline Vector3 ConvertLinearToSRGB(const Vector3& color)
{
    return Vector3{ ConvertLinearToSLuminance(color.x), ConvertLinearToSLuminance(color.y), ConvertLinearToSLuminance(color.z) };
}

inline uint32_t ToColor(const Vector3& vec)
{
    float x = Clampf(vec.x, 0.0f, 1.0f);
    float y = Clampf(vec.y, 0.0f, 1.0f);
    float z = Clampf(vec.z, 0.0f, 1.0f);
    return (uint32_t)(255.0f*x + 0.5f) | ((uint32_t)(255.0f*y + 0.5f) << 8u) | ((uint32_t)(255.0f*z + 0.5f) << 16u) | (255u << 24u);
}


int main(int argc, char* argv[])
{
    if(argc != 4)
    {
        fprintf(stderr, "btf-slice [input-file] [light-slice] [view-slice]");
        return 1;
    }

    auto* btf = LoadBTF(argv[1]);
    if(!btf)
    {
        fprintf(stderr, "Failed to load btf: %s", argv[0]);
        return 1;
    }

    uint32_t light_idx = strtoul(argv[2], nullptr, 10);
    uint32_t view_idx = strtoul(argv[3], nullptr, 10);

    auto area = btf->Width*btf->Height;
    uint32_t* tex_data = new uint32_t[area];

    for(uint32_t btf_y = 0, idx = 0; btf_y < btf->Height; ++btf_y)
    {
       for(uint32_t btf_x = 0; btf_x < btf->Width; ++btf_x, ++idx)
        {
            auto spec = BTFFetchSpectrum(btf, light_idx, view_idx, btf_x, btf_y);
            tex_data[idx] = ToColor(ConvertLinearToSRGB(spec));
        }
    }

    bool ret = SaveTGAImage((uint16_t)btf->Width, (uint16_t)btf->Height, tex_data, "output.tga");
    if(!ret)
    {
        fprintf(stderr, "Failed to save output.tga");
        return 1;
    }

    delete[] tex_data;
    
    DestroyBTF(btf);

    return 0;
}