struct song {
    char *buf;
    size_t len;
};

void alsa_init();
void alsa_play(const char *path);
size_t alsa_offset();
