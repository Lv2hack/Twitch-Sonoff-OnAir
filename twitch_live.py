# ----------------------------------------------------------------
#
#  Twitch online checker
#
#  Requies only a client ID to check online status of a Twitch streamer
#
# ----------------------------------------------------------------




import requests

HEADERS = { 'client-id' : 'putyourclientidhere' }
GQL_QUERY = """
query($login: String) {
    user(login: $login) {
        stream {
            id
        }
    }
}
"""

def isLive(username):
    QUERY = {
        'query': GQL_QUERY,
        'variables': {
            'login': username
        }
    }

    response = requests.post('https://gql.twitch.tv/gql',
                             json=QUERY, headers=HEADERS)
    dict_response = response.json()
    #print()
    #print(dict_response)
    if dict_response['data']['user']:  # If the user exists
        return True if dict_response['data']['user']['stream'] else False
    else:
        return False


if __name__ == '__main__':
    USERS = ['offineandy', 'xqcow', 'baxcast', '2bepwned']
    for user in USERS:
        IS_LIVE = isLive(user)
        print(f'User {user} live: {IS_LIVE}')


