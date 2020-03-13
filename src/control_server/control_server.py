import socket
from pathlib import Path


def load_protocols():
    protocols = {}
    current_protocol = ''

    protocol_file_path = Path(__file__).absolute().parent / 'protocol'
    protocol_file = protocol_file_path.read_text().replace(' ', '')

    for line in protocol_file.splitlines():
        line = line.strip()
        if not line or line.startswith('#endif'):
            continue
        if line.startswith('#ifdef'):
            current_protocol = line.split('_')[-1]
            protocols[current_protocol] = {}
        else:
            k, v = line.split('=')
            protocols[current_protocol][k] = v

    return protocols


def main():
    server = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    server.bind(('127.0.0.1', 9000))

    protocols = load_protocols()
    subscriptions = {name: [] for name in protocols.keys()}

    while True:
        message, address = server.recvfrom(1024)
        print("Received message:", message)

        fields = message.decode('ascii').split(':')
        if len(fields) != 2:
            continue

        type_, value = fields
        type_, value = type_.strip(), value.strip()

        if type_ == 'subscribe' and value in subscriptions.keys():
            subscriptions[value].append(address)
            server.sendto("subscribe : success".encode(), address)

        elif type_ in subscriptions.keys():
            for subscriber in subscriptions[type_]:
                server.sendto((type_ + ' : ' + value).encode(), subscriber)


if __name__ == '__main__':
    main()
