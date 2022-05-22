#include "denoiser.h"

Denoiser::Denoiser() : m_useTemportal(false) {}

void Denoiser::Reprojection(const FrameInfo &frameInfo) {
    int height = m_accColor.m_height;
    int width = m_accColor.m_width;
    Matrix4x4 preWorldToScreen =
        m_preFrameInfo.m_matrix[m_preFrameInfo.m_matrix.size() - 1];
    Matrix4x4 preWorldToCamera =
        m_preFrameInfo.m_matrix[m_preFrameInfo.m_matrix.size() - 2];
#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            m_valid(x, y) = false;
            m_misc(x, y) = Float3(0.f);
            
            int curr_id = frameInfo.m_id(x, y);
            if (curr_id == -1) continue;

            Float3 curr_pos = frameInfo.m_position(x, y);
            Matrix4x4 curr_M = frameInfo.m_matrix[curr_id];
            Matrix4x4 prev_M = m_preFrameInfo.m_matrix[curr_id];
            Float3 prev_pos = preWorldToScreen(
                prev_M(
                    Inverse(curr_M)(
                        curr_pos,
                        Float3::Point),
                    Float3::Point),
                Float3::Point);
            
            int prev_x = prev_pos.x;
            int prev_y = prev_pos.y;
            if (prev_x >= 0 && prev_x < width && prev_y >= 0 && prev_y < height) {
                int prev_id = m_preFrameInfo.m_id(prev_x, prev_y);
                if (prev_id == curr_id) {
                    m_valid(x, y) = true;
                    m_misc(x, y) = m_accColor(prev_x, prev_y);
                }
            }
        }
    }
    std::swap(m_misc, m_accColor);
}

void Denoiser::TemporalAccumulation(const Buffer2D<Float3> &curFilteredColor) {
    int height = m_accColor.m_height;
    int width = m_accColor.m_width;
    int kernelRadius = 3;

#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // clamp C_i-1
            int x_start = std::max(0, x - kernelRadius);
            int x_end = std::min(width - 1, x + kernelRadius);
            int y_start = std::max(0, y - kernelRadius);
            int y_end = std::min(height - 1, y + kernelRadius);
            int N = (x_end - x_start + 1) * (y_end - y_start + 1);

            // mu
            Float3 mu;
            for (int i = x_start; i <= x_end; i++)
                for (int j = y_start; j <= y_end; j++) 
                    mu += curFilteredColor(i, j);
            mu = mu / N;

            // sigma
            Float3 sigma;
            for (int i = x_start; i <= x_end; i++)
                for (int j = y_start; j <= y_end; j++)
                    sigma += SqrDistance(curFilteredColor(i, j), mu);
            sigma = sigma / N;

            Float3 color = Clamp(m_accColor(x, y), mu - sigma * m_colorBoxK, mu + sigma * m_colorBoxK);
            // Exponential moving average
            float alpha = m_valid(x, y) ? m_alpha : 1.0f;
            m_misc(x, y) = Lerp(color, curFilteredColor(x, y), alpha);
        }
    }
    std::swap(m_misc, m_accColor);
}

Buffer2D<Float3> Denoiser::Filter(const FrameInfo &frameInfo) {
    int height = frameInfo.m_beauty.m_height;
    int width = frameInfo.m_beauty.m_width;
    Buffer2D<Float3> filteredImage = CreateBuffer2D<Float3>(width, height);
    int kernelRadius = 16;
#pragma omp parallel for collapse(2)
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int x_start = std::max(0, x - kernelRadius);
            int x_end = std::min(width - 1, x + kernelRadius);
            int y_start = std::max(0, y - kernelRadius);
            int y_end = std::min(height - 1, y + kernelRadius);

            float sum_of_weight = 0;
            Float3 sum_of_weighted_value;

            const Float3 &pos_i = frameInfo.m_position(x, y);
            const Float3 &color_i = frameInfo.m_beauty(x, y);
            const Float3 &normal_i = frameInfo.m_normal(x, y);

            for (int i = x_start; i <= x_end; i++) {
                for (int j = y_start; j <= y_end; j++) {
                    const Float3 &pos_j = frameInfo.m_position(i, j);
                    const Float3 &color_j = frameInfo.m_beauty(i, j);
                    const Float3 &normal_j = frameInfo.m_normal(i, j);

                    float pos = SqrDistance(pos_i, pos_j) / (2.0f * m_sigmaCoord);
                    float color = SqrDistance(color_i, color_j) / (2.0f * m_sigmaColor);
                    float normal = Sqr(SafeAcos(Dot(normal_i, normal_j))) / (2.0f * m_sigmaNormal);
                    float plane = 0.0f;
                    if (pos != .0f) {
                        plane = Sqr(Dot(normal_i, (pos_j - pos_i) / Distance(pos_j, pos_i))) / (2.0f * m_sigmaPlane);
                    }

                    float weight = std::exp(-(pos + color + normal + plane));
                    sum_of_weight += weight;
                    sum_of_weighted_value += color_j * weight;
                }
            }
            filteredImage(x, y) = sum_of_weighted_value / sum_of_weight;
        }
    }
    return filteredImage;
}

void Denoiser::Init(const FrameInfo &frameInfo, const Buffer2D<Float3> &filteredColor) {
    m_accColor.Copy(filteredColor);
    int height = m_accColor.m_height;
    int width = m_accColor.m_width;
    m_misc = CreateBuffer2D<Float3>(width, height);
    m_valid = CreateBuffer2D<bool>(width, height);
}

void Denoiser::Maintain(const FrameInfo &frameInfo) { m_preFrameInfo = frameInfo; }

Buffer2D<Float3> Denoiser::ProcessFrame(const FrameInfo &frameInfo) {
    // Filter current frame
    Buffer2D<Float3> filteredColor;
    filteredColor = Filter(frameInfo);

    // Reproject previous frame color to current
    if (m_useTemportal) {
        Reprojection(frameInfo);
        TemporalAccumulation(filteredColor);
    } else {
        Init(frameInfo, filteredColor);
    }

    // Maintain
    Maintain(frameInfo);
    if (!m_useTemportal) {
        m_useTemportal = true;
    }
    return m_accColor;
}
