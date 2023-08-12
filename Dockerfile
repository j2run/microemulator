FROM alpine:3.18.2

ARG gUser
ARG gId

RUN echo "http://dl-cdn.alpinelinux.org/alpine/edge/testing" >> /etc/apk/repositories
RUN apk update
RUN apk add xvfb fluxbox supervisor bash wqy-zenhei
RUN apk add openjdk8 maven build-base zlib-dev

ADD vnc-config/supervisord.conf /etc/supervisord.conf
ADD vnc-config/config/microemulator/config2.xml /root/.microemulator/config2.xml
ADD vnc-config/entry.sh /entry.sh

COPY vnc-config/jar/game.jar /data/game.jar
COPY vnc-config/jar/password /data/password 
COPY microemulator/vnc/libvncserver.so.0.9.14 /usr/lib/libvncserver.so.0.9.14
COPY microemulator/vnc/libvncserver.so.1 /usr/lib/libvncserver.so.1
COPY microemulator/vnc/libvncserver.so /usr/lib/libvncserver.so

RUN chmod +x /entry.sh

ENV DISPLAY :0
ENV RESOLUTION=1x1
ENV LD_PRELOAD=/usr/lib/libvncserver.so
ENV JAVA_HOME=/usr/lib/jvm/java-8-openjdk/

WORKDIR /data/app

# add user
RUN adduser -D -u ${gId} ${gUser}

# add sudo
RUN apk --no-cache add sudo && \
    echo "${gUser} ALL=(ALL) NOPASSWD: ALL" | tee -a /etc/sudoers

# chown mount volume
RUN mkdir -p /data/app && \
    chown -R ${gUser}:${gUser} /data/app && \
    chown -R ${gUser}:${gUser} /data

EXPOSE 5900

ENTRYPOINT ["bash", "-c", "/entry.sh"]
