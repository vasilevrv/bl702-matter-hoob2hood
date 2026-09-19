.PHONY: bootstrap build flash clean

bootstrap:
	./scripts/bootstrap.sh

build:
	./scripts/build.sh

flash:
	./scripts/flash.sh $(PORT)

clean:
	rm -rf out

