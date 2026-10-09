#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include <chrono>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

class TerminalScopeGuard {
    struct termios old_t_;
public:
    TerminalScopeGuard() {
        tcgetattr(STDIN_FILENO, &old_t_);
        struct termios new_t = old_t_;
        new_t.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &new_t);
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
    }
    ~TerminalScopeGuard() {
        tcsetattr(STDIN_FILENO, TCSANOW, &old_t_);
        int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
        fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK);
    }
};

inline std::string render_oled_screen(int wave_type, float phase_offset, int bpm, int encoder_val, bool button_state, float adc_val) {
    const int width = 32;
    const int height = 7;
    std::vector<std::string> grid(height, std::string(width, ' '));
    
    for (int x = 0; x < width; ++x) {
        float t = (static_cast<float>(x) / width) * 2.0f * 3.14159265f + phase_offset;
        float val = 0.0f;
        switch (wave_type % 4) {
            case 0: val = std::sin(t); break;
            case 1: val = 1.0f - std::fmod(t / 3.14159265f, 2.0f); break;
            case 2: val = (std::sin(t) >= 0.0f) ? 1.0f : -1.0f; break;
            case 3: val = 2.0f * std::abs(2.0f * (t / (2.0f * 3.14159265f) - std::floor(t / (2.0f * 3.14159265f) + 0.5f))) - 1.0f; break;
        }
        int y = static_cast<int>((1.0f - (val + 1.0f) * 0.5f) * (height - 1));
        if (y >= 0 && y < height) grid[y][x] = '*';
    }

    const char* wave_names[] = {"SINE", "SAW", "SQR", "TRI"};
    
    std::string oled = "+--------------------------------+\n";
    char header_buf[64];
    snprintf(header_buf, sizeof(header_buf), "| W:%-4s B:%3d E:%3d%s |\n", wave_names[wave_type%4], bpm, encoder_val, button_state ? "*" : " ");
    oled += header_buf;
    oled += "+--------------------------------+\n";
    
    for (const auto& row : grid) {
        oled += "|" + row + "|\n";
    }
    
    oled += "+--------------------------------+\n";
    char footer_buf[64];
    snprintf(footer_buf, sizeof(footer_buf), "| ADC:%4.2f                       |\n", adc_val);
    oled += footer_buf;
    oled += "+--------------------------------+\n";
    
    return oled;
}

int main() {
    TerminalScopeGuard term_guard;
    bool running = true;
    int current_wave = 0;
    float phase_anim = 0.0f;
    int encoder_val = 50;
    bool button_state = false;
    float adc_expression = 0.0f;
    int current_bpm = 120;
    std::vector<std::chrono::steady_clock::time_point> tap_times;

    std::cout << "\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n";

    while (running) {
        int c = getchar();
        if (c != EOF) {
            if (c == 'w' || c == 'W') {
                current_wave = (current_wave + 1) % 4;
            } else if (c == 's' || c == 'S') {
                current_wave = (current_wave + 3) % 4;
            } else if (c == 'e' || c == 'E') {
                button_state = !button_state;
            } else if (c == 't' || c == 'T') {
                auto now = std::chrono::steady_clock::now();
                if (!tap_times.empty() && std::chrono::duration_cast<std::chrono::milliseconds>(now - tap_times.back()).count() > 2500) {
                    tap_times.clear();
                }
                tap_times.push_back(now);
                if (tap_times.size() > 4) tap_times.erase(tap_times.begin());
                if (tap_times.size() >= 2) {
                    double total_ms = 0;
                    for (size_t i = 1; i < tap_times.size(); ++i) {
                        total_ms += std::chrono::duration_cast<std::chrono::milliseconds>(tap_times[i] - tap_times[i-1]).count();
                    }
                    double avg_ms = total_ms / (tap_times.size() - 1);
                    if (avg_ms > 0) {
                        current_bpm = static_cast<int>(60000.0 / avg_ms);
                        if (current_bpm < 40) current_bpm = 40;
                        if (current_bpm > 300) current_bpm = 300;
                    }
                }
            } else if (c == '1') {
                adc_expression = 0.0f;
            } else if (c == '2') {
                adc_expression = 0.5f;
            } else if (c == '3') {
                adc_expression = 1.0f;
            } else if (c == 'q' || c == 'Q') {
                running = false;
            }
        }

        phase_anim += (current_bpm / 120.0f) * 0.15f;
        encoder_val = (current_wave * 25) + (int)(std::sin(phase_anim) * 5.0f);
        
        std::cout << "\033[18A";
        
        std::cout << "=== PHASE BOX EMBEDDED OLED SIMULATOR ===\n";
        std::cout << render_oled_screen(current_wave, phase_anim, current_bpm, encoder_val, button_state, adc_expression);
        std::cout << "Controls: [w/s] Wave [t] Tap [e] Btn [1-3] ADC [q] Quit\n" << std::flush;

        usleep(50000);
    }

    std::cout << "\nExiting simulator.\n";
    return 0;
}
