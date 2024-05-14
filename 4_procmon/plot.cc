#include "plot.h"

#include <algorithm>
#include <cmath>
#include <gd.h>
#include <gdfonts.h>

struct Plot::MyGdFont : public gdFont {};

Plot::Plot(int width, int height, int totalSeconds, int samplingPeriod)
    : m_width(width)
    , m_height(height)
    , m_totalSeconds(totalSeconds)
    , m_samplingPeriod(samplingPeriod)
    , m_image(gdImageCreate(m_width, m_height))
    , m_font(static_cast<MyGdFont*>(gdFontGetSmall()))
    , m_fontWidth(m_font->w)
    , m_fontHeight(m_font->h)
    , m_background(gdImageColorAllocate(m_image, 255, 255, 240))
    , m_black(gdImageColorAllocate(m_image, 0, 0, 0))
    , m_gray(gdImageColorAllocate(m_image, 200, 200, 200))
    , m_blue(gdImageColorAllocate(m_image, 128, 128, 255))
    , m_kRightMargin(3 * m_fontWidth + 5)
    , m_ratioX(static_cast<double>(m_samplingPeriod * (m_width - s_kLeftMargin - m_kRightMargin)) / m_totalSeconds) {
}

Plot::~Plot() {
    gdImageDestroy(m_image);
}

std::string Plot::plotCpu(const std::vector<double>& data) {
    gdImageFilledRectangle(m_image, 0, 0, m_width, m_height, m_background);
    if (data.size() > 1) {
        gdImageSetThickness(m_image, 2);
        double max = *std::max_element(data.begin(), data.end());
        if (max >= 10.0) {
            max = ceil(max);
        } else {
            max = std::max(0.1, ceil(max * 10.0) /10.0);
        }
        label(max);

        for(size_t i = 0; i < data.size() - 1; ++i) {
            gdImageLine(m_image
                        , getX(i, data.size()), getY(data[i] / max)
                        , getX(i + 1, data.size()), getY(data[i + 1] / max)
                        , m_black);
        }
    }

    int total = m_totalSeconds / m_samplingPeriod;
    gdImageSetThickness(m_image, 1);
    gdImageLine(m_image, getX(0, total), getY(0) + 2, getX(total, total), getY(0) + 2, m_gray);
    gdImageLine(m_image, getX(total, total), getY(0) + 2, getX(total, total), getY(1) + 2, m_gray);
    return toPng();
}

void Plot::label(double maxValue) {
    char buf[64];
    if (maxValue >= 10.0) {
        snprintf(buf, sizeof(buf), "%.0f", maxValue);
    } else {
        snprintf(buf, sizeof(buf), "%.1f", maxValue);
    }

    gdImageString(m_image
            , m_font
            , m_width - m_kRightMargin + 3
            , s_kMarginY - 3
            , reinterpret_cast<unsigned char*>(buf)
            , m_black);

    snprintf(buf, sizeof(buf), "0");
    gdImageString(m_image
            , m_font
            , m_width - m_kRightMargin + 3
            , m_height - s_kMarginY - 3 - m_fontHeight / 2
            , reinterpret_cast<unsigned char*>(buf)
            , m_gray);
    
    snprintf(buf, sizeof(buf), "-%ds", m_totalSeconds);
    gdImageString(m_image
            , m_font
            , s_kLeftMargin
            , m_height - s_kMarginY - m_fontHeight
            , reinterpret_cast<unsigned char*>(buf)
            , m_blue);

    
}

int Plot::getX(ssize_t i, ssize_t total) const {
    double x = (m_width - s_kLeftMargin - m_kRightMargin) + static_cast<double>(i - total) * m_ratioX;
    return static_cast<int>(x + 0.5) + s_kLeftMargin;
}

int Plot::getY(double value) const {
    return static_cast<int>((1.0 - value) * (m_height - 2 * s_kMarginY) + 0.5) + s_kMarginY;
}

std::string Plot::toPng() {
    int size = 0;
    void* png = gdImagePngPtr(m_image, &size);
    std::string result(static_cast<char*>(png), size);
    gdFree(png);
    return result;
}