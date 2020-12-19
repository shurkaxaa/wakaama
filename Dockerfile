FROM alpine:3.12 as builder

RUN apk --no-cache add cmake clang clang-dev make gcc libc-dev linux-headers
WORKDIR build
COPY . .
RUN cmake clean -DCMAKE_BUILD_TYPE=Release examples/server && make

FROM alpine:3.12
COPY --from=builder /build/lwm2mserver /lwm2m-adapter
ENTRYPOINT ["/lwm2m-adapter"]
