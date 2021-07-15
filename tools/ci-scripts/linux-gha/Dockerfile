FROM simonwalton1/hifi_base_ubuntu:1.1
MAINTAINER DevOps Team (devops@highfidelity.io)

EXPOSE 40100 40101 40102
EXPOSE 40100/udp 40101/udp 40102/udp
EXPOSE 48000/udp 48001/udp 48002/udp 48003/udp 48004/udp 48005/udp 48006/udp

RUN mkdir -p /etc/hifi/server/plugins /etc/hifi/server/resources /etc/hifi/server/imageformats/ && \
    ln -s /usr/local/Qt5.12.3/5.12.3/gcc_64/plugins/imageformats/* /etc/hifi/server/imageformats/
COPY ./assignment-client /etc/hifi/server/
#COPY ./oven /etc/hifi/server/
COPY ./domain-server /etc/hifi/server/
RUN true
COPY ./plugins/pcmCodec/libpcmCodec.so /etc/hifi/server/plugins/
# Dummy statement
RUN true
COPY ./*.so /lib/
RUN ln -sf /lib/libquazip5.so /lib/libquazip5.so.1
COPY ./domain-server/resources/ /etc/hifi/server/resources/
RUN true
COPY ./hifi.conf /etc/supervisor/conf.d/hifi.conf
RUN for fn in /usr/local/Qt5.12.3/5.12.3/gcc_64/plugins/imageformats/*.so; do \
    if [ ! -x $fn ]; then ln -s $fn /etc/hifi/server/imageformats; fi; done
RUN chmod +x /etc/hifi/server/domain-server
RUN chmod +x /etc/hifi/server/assignment-client

# Ensure `domain-server` and `assignment-client` execute.
RUN /etc/hifi/server/domain-server --version > /etc/hifi/server/version && \
    /etc/hifi/server/assignment-client --version >> /etc/hifi/server/version

CMD ["/usr/bin/supervisord", "-c", "/etc/supervisor/conf.d/hifi.conf"]
