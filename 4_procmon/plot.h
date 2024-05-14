#pragma once

#include <iostream>
#include <memory>
#include <vector>
#include <stdlib.h>

typedef struct gdImageStruct* gdImagePtr;

class Plot {
public:
    typedef std::shared_ptr<Plot> ptr;

    Plot(int width, int height, int totalSeconds, int samplingPeriod);
    ~Plot();
    std::string plotCpu(const std::vector<double>& data);

private:
    std::string toPng();
private:
    int getX(ssize_t x, ssize_t total) const;
    int getY(double value) const;
    void label(double maxValue);

    struct MyGdFont;
    typedef struct MyGdFont* MyGdFontPtr;

    const int m_width;
    const int m_height;
    const int m_totalSeconds;
    const int m_samplingPeriod;
    gdImagePtr const m_image;
    MyGdFontPtr const m_font;
    const int m_fontWidth;
    const int m_fontHeight;
    const int m_background;
    const int m_black;
    const int m_gray;
    const int m_blue;

    const int m_kRightMargin;
    static const int s_kLeftMargin = 0;
    static const int s_kMarginY = 0;

    const double m_ratioX;
};