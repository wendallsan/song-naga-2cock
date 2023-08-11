


////////////////////////////////////////////////////

#pragma once
#ifndef DSY_NORMALIZATIONPROBE_H
#define DSY_NORMALIZATIONPROBE_H
#ifdef __cplusplus

namespace daisysp {
    class NormalizationProbe {
        public:
            NormalizationProbe();
            ~NormalizationProbe();
            void Init();
            void Disable();
            void High();
            void Low();
            void Write( bool value );

        private:
            void DISALLOW_COPY_AND_ASSIGN( NormalizationProbe );
    };
}
#endif
#endif